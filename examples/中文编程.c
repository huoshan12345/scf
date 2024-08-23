int printf(const char* fmt, ...);

void 打印(const char* 信息)
{
	printf("%s\n", 信息);
}

int main()
{
	const char* 文本 = "你好, 中文编程！\n";

	打印(文本);
	return 0;
}
