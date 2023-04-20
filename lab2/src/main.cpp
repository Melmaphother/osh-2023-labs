#include "shell.h"

int											 flag;
std::vector<pid_t>							 bg_jobs;
std::vector<std::string>					 cmd_history;
std::unordered_map<std::string, std::string> alias_list;
// Hash Map to store instructions and their aliases

int main() {
	// std::cout << "error0 ";
	//  不同步 iostream 和 cstdio 的 buffer
	std::ios::sync_with_stdio(false);
	// std::cout << "error-1 ";
	std::string cmd;
	//  处理CTRL+C
	signal(SIGINT, sigint_handler);
	//  同步历史记录
	//  TODO
	SynchronizeHistory(cmd_history);
	//  处理指令
	while (true) {
		// 打印提示符
		if (!flag) {
			PrintPrompt();
		} else
			flag = 0;
		int d = ctrl_d_handler();
		if (d == 1) { return 0; }
		if (!std::getline(std::cin, cmd)) {
			std::cout << "\n"
					  << "exit" << std::endl;
			return 0;
		}
		flag = 0;

		// 无可处理命令
		if (cmd.empty()) continue;

		// 有可处理命令
		cmd = trim(cmd); // 去除两边的空格

		// 首先判断输入指令是否在重命名表中，在则将化名改为原名
		if (alias_list.find(cmd) != alias_list.end()) { cmd = alias_list[cmd]; }
		// 处理cmd[0] == '!'的特殊命令
		cmd = ExeSpecialCmd(cmd, cmd_history);
		if (cmd.empty()) { return 0; }

		// 将操作写进历史记录中和当前的操作历史表里
		std::ofstream write_file;
		std::string	  path = getenv("HOME") + std::string("/.zsh_cmd_history");
		write_file.open(path.c_str(), std::ios::app);
		if (write_file.is_open()) {
			write_file << cmd << "\n";
			write_file.close();
		} else {
			std::cout << "can not open /.zsh_cmd_history while write history"
					  << std::endl;
			continue;
		}
		cmd_history.push_back(cmd);
		std::vector<std::string> args = split(cmd, " ");
		// exit命令
		if (args[0] == "exit") { // exit命令
			if (args.size() <= 1) { return 0; }

			// std::string 转 int
			std::stringstream code_stream(args[1]);
			int				  code = 0;
			code_stream >> code;

			// 转换失败
			if (!code_stream.eof() || code_stream.fail()) {
				std::cout << "Invalid exit code\n";
				return 1;
			}
			return code;
		}
		if (args[0] == "cd") {
			if (args.size() <= 1) {
				// std::cout << "Insufficient argument\n";
				chdir("/");
				continue;
			}
			int ret = chdir(args[1].c_str());
			if (ret == -1) {
				std::cout << "cd failed" << std::endl;
				// return 2;
			}
			// return 0;
			continue;
		}
		if (args[0] == "alias") {
			if (args.size() <= 1) { return 8; }
			for (size_t i = 1; i < args.size(); i++) {
				size_t equal_pos = args[i].find("=");
				if (equal_pos != std::string::npos) {
					size_t		first_inverted_comma = cmd.find("'");
					size_t		last_inverted_comma	 = cmd.rfind("'");
					std::string alias	  = args[i].substr(0, equal_pos);
					std::string cmd_alias = cmd.substr(
						first_inverted_comma + 1,
						last_inverted_comma - first_inverted_comma - 1);

					alias_list[alias] = cmd_alias;
				}
			}
			continue;
		}
		if (args[0] == "wait") {
			for (size_t i = 0; i < bg_jobs.size(); i++) {
				waitpid(bg_jobs[i], NULL, 0);
				std::cout << bg_jobs[i] << " finished" << std::endl;
			}
			for (size_t i = 0; i < bg_jobs.size(); i++) { bg_jobs.pop_back(); }
			continue;
		}
		bool is_bg = false;
		if (cmd.back() == '&') {
			std::cout << "This is a background command" << std::endl;
			is_bg = true;
			cmd.pop_back(); // 去除末尾的 '&'
			cmd.pop_back(); // 去除末尾的空格
		}
		pid_t pid = fork();
		pid_t p_gid, c_pid, p_grp;
		if (pid == 0) {
			setpgid(pid, pid);
			p_gid	= getpgid(getpid());
			c_pid	= getpid();
			p_grp	= getpgrp();
			int ret = ExePipe(cmd, cmd_history, alias_list);

			if (ret == 0)
				exit(0);
			else {
				std::cout << "error command" << std::endl;
				exit(0);
			}
		} else {
			setpgid(pid, pid);
			p_gid = getpgid(getpid());
			c_pid = getpid();
			p_grp = getpgrp();
			// 信号处理sigaction
			signal(22, SIG_IGN);
			tcsetpgrp(0, pid);
			kill(pid, SIGCONT);
			if (!is_bg) {
				waitpid(pid, NULL, 0); // 前台进程，等待子进程结束
			} else {
				bg_jobs.push_back(pid);
				waitpid(pid, NULL, WNOHANG);
			}
			tcsetpgrp(0, p_gid);
		}
	}
	return 0;
}

/*
以下是上下键切换历史记录的代码，因为过于冗杂而且跟处理ctrl+c有冲突，就忍痛放弃了。
等暑假有时间了再补（可能不补了。。。）
				struct termios old_tio, new_tio;
				int			   command_point = cmd_history.size() - 1;
				// 获取终端属性并备份
				tcgetattr(STDIN_FILENO, &old_tio);
				new_tio = old_tio;

				// 禁用缓冲区和回显
				new_tio.c_lflag &= ~(ICANON | ECHO);
				tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

				char		c;
				std::string input;
				while (true) {
					c = getchar();
					if (c == '\n') { // 换行号停止
						std::cout << "\n";
						break;
					} else if (c == 127) { // 删除号回显
						if (!input.empty()) {
							std::cout << "\b \b";
							cmd = cmd.substr(0, cmd.size() - 1);
							input.pop_back();
							continue;
						} else {
							continue;
						}
					} else if (c == '\003') {
						cmd.clear();
						std::cout << '\n';
						PrintPrompt();
					}

					else if (c == '\004') {
						return 0;
					} else if (c == 27 && getchar() == '[') { // 判断上下键
						switch (getchar()) {
						case 'A': // 上键
							if (command_point > 0) {
								if (command_point < cmd_history.size() - 1) {
									// 非第一次上键，要先退格，再输出
									for (size_t i = 0;
										 i < cmd_history[command_point +
		   1].size();
										 ++i) {
										std::cout << "\b";
									}
									for (size_t i = 0;
										 i < cmd_history[command_point +
		   1].size();
										 ++i) {
										std::cout << " ";
									}
									for (size_t i = 0;
										 i < cmd_history[command_point +
		   1].size();
										 ++i) {
										std::cout << "\b";
									}
									std::cout << cmd_history[command_point];
									cmd = cmd_history[command_point];
input.clear(); input = cmd_history[command_point]; command_point--; } else {
									// 第一次上键，要输出最近的命令
									command_point--;
									std::cout << cmd_history[command_point + 1];
									cmd = cmd_history[command_point + 1];
									input.clear();
									input = cmd_history[command_point + 1];
								}
							}
							break;
						case 'B': // 下键
							if (command_point < cmd_history.size() - 2) {
								command_point++;
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << "\b";
								}
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << " ";
								}
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << "\b";
								}
								std::cout << cmd_history[command_point + 1];
								cmd = cmd_history[command_point + 1];
								input.clear();
								input = cmd_history[command_point + 1];
							} else if (command_point == cmd_history.size() - 2)
		   {
								// 上一次命令是最后一个命令，下键要退格且不输出
								command_point++;
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << "\b";
								}
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << " ";
								}
								for (size_t i = 0;
									 i < cmd_history[command_point].size(); ++i)
		   { std::cout << "\b";
								}
								input.clear();
							} else {
								// 第一次下键，不输出
								input.clear();
							}
							break;
						default:
							input.clear();
							break;
						}
					} else {
						std::cout << c;
						cmd += c;
						input.push_back(c);
					}
				}

				// 恢复终端属性
				tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);
		*/