// IO
#include <iostream>
// File
#include <fstream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::unordered_map
#include <unordered_map>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// open
#include <fcntl.h>
// wait
#include <sys/wait.h>
// getpwuid
#include <sys/types.h>
// getpwuid
#include <pwd.h>
//
#include <termios.h>
//
#include <algorithm>

extern int				  flag;
extern std::vector<pid_t> bg_jobs;
int						  ctrl_d_handler();
void					  PrintPrompt();
void					  sigint_handler(int);
int SynchronizeHistory(std::vector<std::string> &cmd_history);
int ExePipe(std::string &cmd, std::vector<std::string> &cmd_history,
			std::unordered_map<std::string, std::string> &alias_list);
int ExeCmdWithoutPipes(
	std::vector<std::string> &args, std::vector<std::string> &cmd_history,
	std::unordered_map<std::string, std::string> &alias_list);
std::vector<std::string> split(std::string s, const std::string &delimiter);

std::string ExeSpecialCmd(std::string			   &cmd,
						  std::vector<std::string> &cmd_history);
int			ExeBuildinCmd(std::vector<std::string>					   &args,
						  std::vector<std::string>					   &cmd_history,
						  std::unordered_map<std::string, std::string> &alias_list);
int			ExeExternalCmd(std::vector<std::string> &args);
int		   *ExeCommandWithReDir(std::vector<std::string> &args);
int			ExeSingleCommand(std::vector<std::string> &args, int *fd);
std::vector<std::string> split(std::string s, const std::string &delimiter);
std::string trim(std::string cmd);