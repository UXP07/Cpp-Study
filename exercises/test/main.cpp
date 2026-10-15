// test —— 学习草稿区（非正式练习，随手试验代码的地方）
// 构建：cmake --build --preset default --target test
// 运行：./build/bin/test.exe
#include <iostream>
#include <thread>

void hello(){
    std::cout<< " hello from thread " <<std::endl;
}

int main() {
    int a = 10;
    int& b = std::ref(a);
    b+=1;
    printf("%d\n", a);
    printf("%d\n", b);
    
    return 0;
}
