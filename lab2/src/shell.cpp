#include "shell.h"

void sigint_handler(int) {
	std::cout << '\n';
	// cmd.clear();
	PrintPrompt();
	flag = 1;
	std::cout.flush();
}
int ctrl_d_handler() {
	if (feof(stdin)) return 1;
	char c;
	c = std::cin.get();
	if (c == EOF)
		return 1;
	else {
		std::cin.putback(c);
		return 0;
	}
}
/**
 * @brief print "username:path: # "
 * @return void
 */
void PrintPrompt() {
	uid_t		   uid = geteuid();
	struct passwd *pw  = getpwuid(uid);
	std::cout << pw->pw_name;
	std::cout << "@";
	std::string path;
	path.resize(PATH_MAX); // 提前设置path大小
	char *ret = getcwd(&path[0], PATH_MAX);

	if (ret == nullptr) { // 函数执行失败
		std::cout << "printprompt failed";
	} else {
		std::cout << ret << ": ";
	}
	std::cout << "# ";
}
/**
 * @brief synchronize history
 *
 * @param cmd_history history of commands
 * @return 0 if success
 * @return 1 if fail to read cmd_history
 */
int SynchronizeHistory(std::vector<std::string> &cmd_history) {
	std::ifstream read_file;
	std::string	  lineStr;
	std::string	  path = getenv("HOME") + std::string("/.zsh_cmd_history");
	read_file.open(path.c_str());
	if (read_file.is_open()) {
		while (getline(read_file, lineStr)) { cmd_history.push_back(lineStr); }
		read_file.close();
		return 0;
	} else {
		std::cout << "can not open ./zsh_cmd_history while sychronize history"
				  << std::endl;
		return 1;
	}
}

/**
 * @brief handle special cmd
 *
 * @param cmd command
 * @param cmd_history history of commands
 * @return str::string
 * @return initial command if cmd[0] != '!'
 * @return commands from history
 */
std::string ExeSpecialCmd(std::string			   &cmd,
						  std::vector<std::string> &cmd_history) {
	if (cmd[0] == '!') { //! do not record to history
		int size = cmd_history.size();
		if (cmd[1] == '!') {
			cmd = cmd_history[size - 1];
			// TODO
			std::cout << cmd << std::endl;
		} else {
			// std::string 转 int
			std::stringstream code_stream(cmd.substr(1));
			int				  code = 0;
			code_stream >> code;

			// 转换失败
			if (!code_stream.eof() || code_stream.fail()) {
				std::cout << "Invalid ! code\n";
				return NULL;
			} else if (code > size) {
				std::cout << "! code is too large";
			} else {
				cmd = cmd_history[code - 1];
				std::cout << cmd << std::endl;
			}
		}
	}
	return cmd;
}

/**
 * @brief handle pipes
 *
 * @param cmd
 * @param cmd_history
 * @param alias_list
 * @return int
 * @return 0 if success
 * @return -x if fail
 */
int ExePipe(std::string &cmd, std::vector<std::string> &cmd_history,
			std::unordered_map<std::string, std::string> &alias_list) {
	//
	std::vector<std::string> args_pipe = split(cmd, " | ");
	if (args_pipe.empty()) { return -1; }

	int pipe_size = args_pipe.size();
	// std::cout << pipe_size;
	if (pipe_size == 1) { // 没有管道
		std::vector<std::string> args = split(cmd, " ");
		if (ExeCmdWithoutPipes(args, cmd_history, alias_list) == 0) return 0;
		return -1;
	} else if (pipe_size == 2) { // 一个管道
		int fd[2];
		int ret = pipe(fd);
		if (ret == -1) {
			std::cout << "can not create pipe";
			return -2;
		}
		pid_t pid = fork(); // fork一个新进程
		if (pid == 0) {
			dup2(fd[1], STDOUT_FILENO); // 将子进程标准输出端与管道的写端相连
			close(fd[0]);
			close(fd[1]); // 关管道写端
			std::vector<std::string> args_1 = split(args_pipe[0], " ");
			ExeCmdWithoutPipes(args_1, cmd_history, alias_list);
			exit(0);
		} else {
			pid = fork();
			if (pid == 0) {
				// 子进程2：将标准输入重定向到管道的读端
				dup2(fd[0], STDIN_FILENO); // 将子进程标准输入端与管道的读端相连
				close(fd[1]);
				close(fd[0]); // 关管道读端
				std::vector<std::string> args_2 = split(args_pipe[1], " ");
				ExeCmdWithoutPipes(args_2, cmd_history, alias_list);
				exit(0); // 否则无法结束子进程
			} else {
				// 父进程：关闭管道的读端和写端，并等待子进程结束
				close(fd[0]);
				close(fd[1]);
				while (wait(NULL) > 0)
					; // 等待所有子进程结束
			}
		}

	} else { // 大于一个管道
		int last_pipe_read_port = -1;
		for (int i = 0; i < pipe_size; i++) {
			int fd[2];
			if (i < pipe_size - 1) { // 只需要构造pipe_size - 1个管道
				int ret = pipe(fd);
				if (ret == -1) {
					std::cout << "can not create pipe";
					return -2;
				}
			}
			pid_t pid = fork();
			if (pid == 0) { // 子进程
				// 这里不能关闭管道的读端，否则在父进程中无法记录当前管道的读口
				if (i < pipe_size - 1) {
					// 将子进程的标准输出与管道的写端相连
					dup2(fd[1], STDOUT_FILENO);
					// 关管道写端
					close(fd[1]);
				}
				if (i != 0) {
					// 将上一个管道的读端与当前子进程的标准输入相连
					dup2(last_pipe_read_port, STDIN_FILENO);
				}
				std::vector<std::string> args = split(args_pipe[i], " ");
				ExeCmdWithoutPipes(args, cmd_history, alias_list);
				exit(0);
			} else { // 父进程

				if (i != 0) {
					close(last_pipe_read_port);
					// 跟只有一个管道的操作关的端口其实一样
				}
				close(fd[1]); // 关管道的写口
				last_pipe_read_port = fd[0]; // 父进程中将当前管道的读口保存下来
			}
		}
		while (wait(NULL) > 0)
			; // 等待所有子进程结束
	}
	return 0;
}

/**
 * @brief handle without pipes
 *
 * @param args
 * @param cmd_history
 * @param alias_list
 * @return int
 * @return 0 if success
 * @return -x if fail
 */
int ExeCmdWithoutPipes(
	std::vector<std::string> &args, std::vector<std::string> &cmd_history,
	std::unordered_map<std::string, std::string> &alias_list) {
	int buildin_cmd_result = ExeBuildinCmd(args, cmd_history, alias_list);
	if (buildin_cmd_result == -2) {
		// 是外部命令
		int external_cmd_result = ExeExternalCmd(args);
		if (external_cmd_result == -1) { // 外部命令处理错误
			return -2;
		} else { // 外部命令处理正确
			return 0;
		}
	} else if (buildin_cmd_result == 0) { // 是内建命令中除exit命令的其他命令
		return 0;
	} else {
		std::cout << "error in buildincmd" << std::endl;
		return -1;
	}
}

/**
 * @brief handle buildin cmd
 *
 * @param args splited command
 * @param cmd_history history of commands
 * @param alias_list alias hash map
 * @return int
 * @return 0 if success
 * @return -1 if this cmd is not buildin_cmd
 * @return positive number if there is some errors when handle cmd
 */
int ExeBuildinCmd(std::vector<std::string>					   &args,
				  std::vector<std::string>					   &cmd_history,
				  std::unordered_map<std::string, std::string> &alias_list) {

	// pwd命令
	if (args[0] == "pwd") {
		// 在<climits>中定义了PATH_MAX常量
		if (args.size() != 1) {
			std::cout << "Insufficient argument\n";
			return 3;
		}
		std::string path;
		path.resize(PATH_MAX); // 提前设置path大小
		char *ret = getcwd(&path[0], PATH_MAX);

		if (ret == nullptr) { // 函数执行失败
			std::cout << "pwd failed";
			return 4;
		} else {
			std::cout << ret << std::endl;
			return 0;
		}
	} else if (args[0] == "wait") {
		for (size_t i = 0; i < bg_jobs.size(); i++) {
			waitpid(bg_jobs[i], NULL, 0);
			std::cout << bg_jobs[i] << " finished" << std::endl;
		}
		for (size_t i = 0; i < bg_jobs.size(); i++) { bg_jobs.pop_back(); }
		return 0;
	}
	//
	else if (args[0] == "history") {
		int size = cmd_history.size();
		if (args.size() <= 1) {
			for (int i = 1; i < size; i++) {
				std::cout << "  " << i + 1 << "  " << cmd_history[i]
						  << std::endl;
			}
		} else {
			// std::string 转 int
			std::stringstream code_stream(args[1]);
			int				  code = 0;
			code_stream >> code;

			// 转换失败
			if (!code_stream.eof() || code_stream.fail()) {
				std::cout << "Invalid history code" << std::endl;
				return 6;
			}

			if (code > size + 1 | code <= 0) {
				std::cout << "parameter is invalid" << std::endl;
				return 7;
			}
			for (int i = code - 1; i < size; i++) {
				std::cout << "  " << i + 1 << "  " << cmd_history[i]
						  << std::endl;
			}
		}
		return 0;
	}
	//

	//
	else if (args[0] == "echo") {
		if (args.size() <= 1) { return 1; }
		if (args[1][0] == '~') {
			std::string	   home_path = getenv("HOME");
			uid_t		   uid		 = geteuid();
			struct passwd *pw		 = getpwuid(uid);
			std::string	   pwname	 = pw->pw_name;
			for (size_t i = 1; i < args.size(); ++i) {
				if (args[i][0] == '~') {
					std::string param = args[i].substr(1);
					if (param == "") {
						std::cout << home_path
								  << ((i < args.size() - 1) ? " " : "\0");
					} else if (param == "root") {
						std::cout << "/root"
								  << ((i < args.size() - 1) ? " " : "\0");
					} else if (param == pwname) {
						std::cout << home_path
								  << ((i < args.size() - 1) ? " " : "\0");
					} else {
						std::cout << home_path << "/" << param
								  << ((i < args.size() - 1) ? " " : "\0");
					}
				} else {
					std::cout << "error parameter of ~";
				}
			}
			std::cout << std::endl;
		} else if (args[1][0] == '$') {
			for (size_t i = 1; i < args.size(); ++i) {
				if (args[i][0] == '$') {
					std::string param = args[i].substr(1);
					std::cout << getenv(param.c_str())
							  << ((i < args.size() - 1) ? " " : "\0");
				} else {
					std::cout << "error parameter of $";
				}
			}
			std::cout << std::endl;
		} else {
			for (size_t i = 1; i < args.size(); i++) {
				std::cout << args[i] << ((i < args.size() - 1) ? " " : "\0");
			}
			std::cout << std::endl;
		}
		return 0;
	} else
		return -2;
}

/**
 * @brief handle external command
 *
 * @param args
 * @ref https://blog.csdn.net/feng964497595/article/details/80297318
 * @return int
 * @return 0 if success
 * @return -1 if fail
 */
int ExeExternalCmd(std::vector<std::string> &args) {
	int *fd = ExeCommandWithReDir(args);
	return ExeSingleCommand(args, fd);
}

/**
 * @brief handle without redir
 *
 * @param args
 * @return int*
 * @return fd[0] and fd[1]
 */
int *ExeCommandWithReDir(std::vector<std::string> &args) {
	int *fd;
	fd		 = new int[2]; // 需要提前分配内存，不然make时报错
	fd[0]	 = STDIN_FILENO;
	fd[1]	 = STDOUT_FILENO;
	int size = args.size();
	int i	 = size - 1;
	while (i >= 0) {
		if (args[i] == "<") {			  // READ
			args.erase(args.begin() + i); // 删除“<”字符
			fd[0] = open(args[i].c_str(), O_RDONLY);
			args.erase(args.begin() + i, args.end());
			size = args.size(); // 重改向量的大小

		} else if (args[i] == ">") {	  // WRITE
			args.erase(args.begin() + i); // 删除“>”字符
			fd[1] = open(args[i].c_str(), O_WRONLY | O_CREAT | O_TRUNC,
						 S_IRUSR | S_IWUSR);
			args.erase(args.begin() + i, args.end());
			size = args.size();
		} else if (args[i] == ">>") {	  // APPEND
			args.erase(args.begin() + i); // 删除“>>”字符
			fd[1] = open(args[i].c_str(), O_WRONLY | O_CREAT | O_APPEND,
						 S_IRUSR | S_IWUSR);
			args.erase(args.begin() + i, args.end());
			size = args.size();
		}
		i--;
	}
	return fd;
}

/**
 * @brief handle without redir(single command)
 *
 * @param args
 * @param fd
 * @return int
 * @return 0 if success
 * @return -1 if fail
 */
int ExeSingleCommand(std::vector<std::string> &args, int fd[]) {

	// std::vector<std::string> 转 char **
	char *arg_ptrs[args.size() + 1];
	for (size_t i = 0; i < args.size(); i++) { arg_ptrs[i] = &args[i][0]; }
	// exec p 系列的 argv 需要以 nullptr 结尾
	arg_ptrs[args.size()] = nullptr;

	dup2(fd[0], STDIN_FILENO);
	dup2(fd[1], STDOUT_FILENO); // 将命令与重定向文件连接
	if (execvp(args[0].c_str(), arg_ptrs) == -1) { return -1; }
	return 0;
}

/**
 * @brief split command
 *
 * @param s
 * @param delimiter
 * @return std::vector<std::string>
 * @return cmd that is splited
 */
std::vector<std::string> split(std::string s, const std::string &delimiter) {
	std::vector<std::string> res;
	size_t					 pos = 0;
	std::string				 token;
	while ((pos = s.find(delimiter)) != std::string::npos) {
		token = s.substr(0, pos);
		res.push_back(token);
		s = s.substr(pos + delimiter.length());
	}
	res.push_back(s);
	return res;
}

/**
 * @brief trim " "、"\n"、"\t"、"\r"
 *
 * @param cmd
 * @return std::vector<std::string>
 * @return command that is trimed
 */
std::string trim(std::string cmd) {
	if (!cmd.empty()) {
		cmd.erase(0, cmd.find_first_not_of(" \n\t\r"));
		cmd.erase(cmd.find_last_not_of(" \n\t\r") + 1);
	}
	return cmd;
}