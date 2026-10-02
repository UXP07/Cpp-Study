// test —— 学习草稿区（非正式练习，随手试验代码的地方）
// 构建：cmake --build --preset default --target test
// 运行：./build/bin/test.exe
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdio>

// class FileGuard{

// };

class Shallow{
public:
    int *data;
public:
    Shallow() : data(nullptr) {}
    Shallow(int v) : data(new int(v)) {}
    Shallow(const Shallow& other) : data(new int(*(other.data))) {}

    Shallow& operator=(const Shallow& other){
        if(this != &other)
        {
            int *tmp = new int(*other.data);
            delete data;
            data = tmp;
        }

        return *this;
    }

    int getdata() const {
       return *data; 
    }

    ~Shallow() {delete data;}
};

int main() {

    Shallow s1(10);
    Shallow s2 = s1;

    printf("%p\n",s1.data);
    printf("%p\n",s2.data);
    printf("%d\n",*s1.data);
    printf("%d\n",*s2.data);

    *(s2.data) = 20;

    printf("%d\n",*s1.data);
    printf("%d\n", s2.getdata());
    return 0;
}
