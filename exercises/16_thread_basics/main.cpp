#include <cstddef>
#include <thread>
#include <utility>
#include <vector>

/*
SR1. `std::thread t(f);` 之后既不 join 也不 detach 就让 t 析构——会发生什么
     （精确到程序的行为）？join 和 detach 的本质区别？detach 出去的线程
     还引用着函数里的局部变量，函数返回后会怎样（这正是 RAII 视角下
     detach 的什么问题）？
    1、导致程序终止杀进程。
    2、join是原地阻塞等待线程结束，detach是直接放线程自主运行不再接管。
    3、需要看局部变量的生命周期能不能覆盖线程，如果在detach的线程还没结束
       运行之前返回，那么局部变量被释放，线程再用它就是内存错误。
       RAII 命名：这是"线程的生命周期超过了它所用资源的生命周期"——反 RAII
      （RAII 要求资源与对象同生共死，detach 把顺序整个倒了过来）。
SR2. 传给 thread 构造的实参经历了什么（两步：先拷进 thread 内部、
     运行期再转给函数）？忘写 std::ref 编译能过吗、现象是什么——
     为什么说它比编译错误更危险？
    1、先拷贝一份在thread内部，然后运行期转给函数。
    2、能过，但是以拷贝的方式传入线程，并不操作原有变量——比编译错误
       危险的重锤在"静默"：没有任何报警，程序看起来全对，只有结果不对。
       编译错误会拦你，它只会骗你。
SR3. thread_local 变量有几个副本、归谁、什么时候生什么时候死？
     多个线程同时写各自的 thread_local 副本算数据竞争吗？
     什么场景用它比"全局变量 + 锁"更合适？
    1、有几个线程调用就有几个副本，每个副本归所在的线程管辖，随线程创建生，线程销毁死。
    2、不算数据竞争，相互独立。
    3、各自计数的场景，无需写入竞争性资源的场景。
SR4. parallel_sum 为什么让每个线程写自己的槽位而不是共享一个总和变量
     （共享 += 是什么性质的错误；锁能救但代价是什么）？
     "分片-汇总"这个模式在低延迟世界叫什么名字、你能在哪些地方再用它？
    1、写自己的槽位从结构上消灭竞争。定性先于效率：共享 += 是数据竞争、
       未定义行为——不是"慢"，是"错"（读改写被中途打断，丢更新只是
       它最温柔的表现）；锁能救但每次 += 都付拿放锁的价，把并行的
       好处吃光。
    2、这个模式叫 sharding（分片），map-reduce 的雏形。线程池任务分派、
       SPSC 队列、分片行情处理都是它。
*/

long long sum_slice(const std::vector<int>& v, std::size_t begin, std::size_t end)
{
    long long sum = 0;
    for(std::size_t i=begin; i<end; i++)
    {
        sum += v[i];
    }
    return sum;
}

long long parallel_sum(const std::vector<int>& v, unsigned nthreads){
    std::vector<long long> partials(nthreads, 0);
    std::vector<std::thread> th;
    th.reserve(nthreads);
    
    long long sum = 0;

    const std::size_t base = v.size()/nthreads;
    const std::size_t extra = v.size()%nthreads;
    std::size_t begin = 0;
    
    for(unsigned int i=0; i<nthreads; i++)
    {
        const std::size_t len = base + (i < extra ? 1 : 0);
        th.emplace_back([&v, &partials, i, begin, len]() {
            partials[i] = sum_slice(v, begin, begin+len);
        });
        begin += len;
    }

    for(auto& t : th) t.join();

    for(long long p : partials) sum += p;

    return sum;
}

void add_many(int& v, int times)
{
    for(int i=0; i<times; i++) v++;
}

void bump_via_thread(int& x)
{
    std::thread t(add_many, std::ref(x), 1000);
    t.join();
}

thread_local int tl_ticks = 0;
void tick_worker(int n){
    tl_ticks += n;
}

int my_ticks() { return tl_ticks; }



int main(){
    return 0;
}