#include <new>
#include <utility>

template <class T>
class Vec{
public:

    Vec() = default;

    Vec(const Vec&) = delete;
    Vec& operator=(const Vec&) = delete;

    Vec(Vec&& other) noexcept : buf_(other.buf_), size_(other.size_), cap_(other.cap_) {
        other.steal_reset();
    }

    Vec& operator=(Vec&& other) noexcept {
        if(this != &other)
        {
            destroy_all();
            buf_ = other.buf_;
            size_ = other.size_;
            cap_ = other.cap_;
            other.steal_reset();
        }

        return *this;
    }

    ~Vec()
    {
        destroy_all();
    }

    void reserve(std::size_t new_cap) {
        if(new_cap <= cap_)
        {
            return ;
        }

        reallocate(new_cap);
    }

    void push_back(T&& v) {
        ensure_space();
        ::new (buf_+size_) T(std::move(v));
        size_++;
    }
    void push_back(const T& v) {
        ensure_space();
        ::new (buf_+size_) T(v);
        size_++;
    }

    T& operator[](std::size_t i) { return buf_[i]; }
    const T& operator[](std::size_t i) const { return buf_[i]; }
    std::size_t size() const { return size_; }
    std::size_t capacity() const { return cap_; }

private:
    T* buf_ = nullptr;
    std::size_t size_ = 0;
    std::size_t cap_ = 0;

    void steal_reset() {
        buf_ = nullptr;
        size_ = 0;
        cap_  = 0;
    }

    static void release_buffer(T* p, std::size_t n)
    {
        for(std::size_t i=0; i<n; i++)
            p[i].~T();

        ::operator delete(p);
    }

    void destroy_all() {
        release_buffer(buf_, size_);
        steal_reset();       
    }

    void reallocate(std::size_t new_cap)
    {
        T* nb = static_cast<T*>(::operator new(new_cap*sizeof(T)));
        for(std::size_t i=0; i<size_; i++)
        {
            ::new (nb+i) T(std::move_if_noexcept(buf_[i]));
        }
        release_buffer(buf_, size_);
        buf_ = nb;
        cap_ = new_cap;
    }

    void ensure_space(){
        if(size_ == cap_){
            reallocate(cap_ ? cap_*2 : 1);
        }
    }



};


int main()
{
    return 0;
}