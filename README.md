# scf
-------
#### 介绍
一个完全自己编写的编译器框架,

1，支持类C语法，强类型，编译型（直接生成机器码的可执行文件），

2，支持Linux系统、x64和arm64平台，

3，支持多值函数，

4，支持自动内存管理（x64版本），arm64的还没添加，

5，支持用户自定义模块，并在它的基础上进行二次开发（创造自己的编程语言）。
-----
a Simple Compiler Framework written by me completely.

1，support syntax like C, strong type, compile to exec file with native code.

2，support Linux and x86_64 and arm64 (aarch64),

3，support function with multi-return-values,

4，support auto memory management in x86_64 (will support in arm64 later),

5，support user designed modules, so you can develop your own program language.
------
#### 软件架构

#### 安装教程, usage

1.  用git下载到本机，命令为：

git clone http://baseworks.info/gitweb/scf.git

download it with git, command above.

2.  在scf/parse目录下直接执行make，即可获得编译器的可执行文件scf，命令为：

cd scf/parse

make

in directory scf/parse run make, then get the executable file named 'scf'.

3.  写一段示例代码，例如：

int printf(const char* fmt);

int main()

{

    printf("hello world\n");

    return 0;


}

保存为文件hello.c，

write a code like above, and save it as 'hello.c'.

4，然后用scf编译它, 即可获得可执行文件(默认文件名是1.out), 命令为:

scf hello.c

compile it with scf, then get the executable file '1.out' of your code.

5，给它加上可执行权限：

chmod +x 1.out

give 1.out executable priority.

6，然后执行它：

./1.out

即可看到打印的"hello world".

run it, and can see the words "hello world" in shell.

7，源代码的扩展名建议用.c，虽然编译器会把.o,.a,.so之外的"任何文件"都当成源码的文本文件，但.c在大多数"编辑器"里都有"语法提示"。

the extended name of source code should be .c, 

though every name except .o,.a,.so will be considered as the source code,

but .c is supported by all editors in syntax high-lighting.

8，scf对源文件扩展名的检测在main.c里，你可以把第182行的.c改成任何你想要的扩展名:( 但不能是.a,.so,.o。

scf checks the extended name in Line 182 of main.c, you can revise to 'any' you want, except .a,.so,.o.

9，details in the code:(

细节在源码里:(

#### 参与贡献

yu.dongliang (底层技术栈)
