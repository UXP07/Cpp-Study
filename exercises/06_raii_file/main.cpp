#include <cstdio>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>   // std::move 的正式居所（此前靠 iostream 间接引入，不保险）

/*
SR1. 构造函数抛异常时，析构函数为什么不会被调用？已构造完成的成员会发生什么？
     这如何保证"构造失败必不泄漏"？若构造函数要先后获取两份资源，第二份失败怎么办？
答： 1、构造函数没跑完，对象的生命周期从未开始；析构函数只作用于完整构造的对象，
        所以不会被调用。
     2、已构造完成的成员会按构造的逆序自动析构。
     3、"资源获取于构造"意味着：要么对象存在且持有资源，要么对象根本不存在——
        不存在"抓着资源的半成品"，因此构造失败必不泄漏。
     4、两份资源：每份资源交给一个 RAII 成员持有，第二份失败抛异常时，
        已构造成员的自动析构会释放第一份；徒手管理则需 try/catch 手动放第一份。
SR2. `std::move` 本身移动了任何东西吗？它到底做了什么？
     为什么移动构造/赋值必须把源对象句柄置空，不置空会发生什么？
答： 1、什么都不移动。std::move 就是 static_cast<T&&>，纯编译期类型转换，
        唯一作用是让重载决议选中移动版本；真正的移动发生在移动构造/赋值的函数体里。
     2、移动构造接管地址后，源和目标暂时指向同一资源；源析构时若还拿着旧指针
        就会 fclose，目标析构时再 fclose = double free（未定义行为）。
        置空是给源对象的析构"拆雷"——后果是双重释放，与泄漏无关。
SR3. 移动构造/移动赋值为什么要标 `noexcept`？提示：`std::vector<LogFile>` 扩容时，
     元素搬迁在"移动构造有无 noexcept"下分别走哪条路径，为什么？
答： 1、noexcept 不是防护罩，是承诺书：函数体照样可能抛异常，但异常一旦试图
        逃出 noexcept 函数，运行期直接 std::terminate——改变的是逃逸的后果。
     2、无 noexcept：扩容搬一半抛异常无法回滚（旧元素已被掏空），vector 为保
        强异常安全只能退化为逐个拷贝；有 noexcept：放心用移动。
        一句话：搬一半炸了回不去，拷一半炸了能复原。
SR4. 什么是栈展开，发生在什么时机？结合本次运行，指出哪两行输出的先后顺序
     证明了栈展开发生；从 throw 到进入 catch 之间发生了什么？
答： 1、栈展开是 C++ 运行时（不是 OS）的异常处理过程：从 throw 到进入匹配的
        catch 之间，调用栈从 throw 点逐层退出。
     2、每退出一层，该层"已完整构造"的局部对象按逆序自动析构，直到某层 catch
        匹配上。证据：驱动输出中 "[LogFile] dtor: unwind.log (closed)" 出现在
        "[check] caught: simulated failure" 之前——异常还在飞行途中，文件已关闭。
 */



class LogFile
{
private:
    /* data */
    FILE * fp_;
    std::string fileName_;
    size_t byteNum_;

public:
    LogFile(std::string fileName, std::string mode) : fp_(fopen(fileName.c_str(), mode.c_str())), fileName_(fileName), byteNum_(0) {
        if(!fp_)
        {
            throw std::runtime_error("Open file failed: "+fileName_);            
        }
        std::cout << "[LogFile] ctor: " << fileName_ << std::endl;
    }

    LogFile(const LogFile& other) : fp_(fopen(other.getFileName().c_str(), "a")), fileName_(other.getFileName()), byteNum_(0)
    {
        if(!fp_)
        {
            throw std::runtime_error("Open file failed: "+fileName_);
        }
        std::cout << "[LogFile] copy-ctor: " << fileName_ << std::endl;
    }

    LogFile& operator=(const LogFile & other)
    {
        std::string oldName = fileName_;   // 声明即初始化：自赋值分支也有正确的旧名
        if(this != &other)
        {
            FILE* fresh = fopen(other.getFileName().c_str(), "a");
            if(!fresh) 
            {
                throw std::runtime_error("Open file failed: "+fileName_);
            } 

            if(fp_) fclose(fp_);
            fp_ = fresh;
            byteNum_ = 0;
            fileName_ = other.getFileName();
        }
        std::cout << "[LogFile] copy-assign: " << oldName << " -> " << fileName_ << std::endl;
        return *this;
    }

    LogFile(LogFile&& other) noexcept : fp_(other.fp_), fileName_(std::move(other.fileName_)), byteNum_(other.getByteNum())
    {
        other.fp_ = nullptr;
        other.byteNum_ = 0;
        std::cout << "[LogFile] move-ctor: " << fileName_ << std::endl;
    }

    LogFile& operator=(LogFile&& other) noexcept
    {
        std::string oldName = fileName_;   // 同上：声明即初始化
        if(this != &other)
        {
            if(fp_) fclose(fp_);
            fp_ = other.fp_;
            other.fp_ = nullptr;

            byteNum_ = other.getByteNum();
            other.byteNum_ = 0;

            fileName_ = std::move(other.fileName_);
        }
        std::cout << "[LogFile] move-assign: " << oldName << " -> " << fileName_ << std::endl;
        return *this;
    }


    size_t write(const std::string& str)
    {
        if(fp_ == nullptr) return 0;
        size_t ret =  fwrite(str.c_str(), sizeof(char), str.size(), fp_);
        fflush(fp_);
        byteNum_ += ret;
        return ret;
    }

    const std::string& getFileName() const
    {
        return fileName_;
    }

    bool isFile() const { return fp_ != nullptr; }

    size_t getByteNum() const { return byteNum_; }




    ~LogFile() {
        if(fp_ != nullptr)
        {
            fclose(fp_);
            std::cout << "[LogFile] dtor: " + fileName_ + " (closed)" << std::endl;
        }
        else
        {
            std::cout << "[LogFile] dtor: (empty)" << std::endl;
        }
    }
};

int main()
{



    return 0;
}
