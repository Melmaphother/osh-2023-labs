#include <iostream>
#include <termios.h>
#include <unistd.h>

using namespace std;

int main() {
	struct termios old_tio, new_tio;
	cout << "# ";
	std::string a = "abc";
	cout << a.size() << endl;
	// 获取终端属性并备份
	char d;
	d = getchar();
	if (d == 3) { std::cout << "true"; }
	tcgetattr(STDIN_FILENO, &old_tio);
	new_tio = old_tio;

	// 禁用缓冲区和回显
	new_tio.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

	char c;
	while ((c = getchar()) != '\n') {
		if (c == 27 && getchar() == '[') { // 判断上下键
			switch (getchar()) {
			case 'A': // 上键
				cout << "\b\b\b\b    \b\b\b\b";
				cout << "cd";
				break;
			case 'B': // 下键
				cout << "\b\b\b\b    \b\b\b\b";
				cout << "pw";
				break;
			default:
				break;
			}
		} else {
			putchar(c);
		}
	}

	// 恢复终端属性
	tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

	return 0;
}
