#include <cstddef>
#include <string_view>
#include <stdexcept>
#include <vector>
#include <array>

// 表示一个不可修改的字节视图，提供对内存中连续字节的只读访问
struct bytes_const_view {
    // 指向字节数据的指针
    char const *m_data;
    // 数据大小
    size_t m_size;

    // 返回数据指针
    char const *data() const noexcept {
        return m_data;
    }

    // 返回数据大小
    size_t size() const noexcept {
        return m_size;
    }

    // 返回数据开始的迭代器
    char const *begin() const noexcept {
        return data();
    }

    // 返回数据结束的迭代器
    char const *end() const noexcept {
        return data() + size();
    }

    // 获取一个子视图，从指定位置开始，长度为 len
    bytes_const_view subspan(size_t start,
                             size_t len = static_cast<size_t>(-1)) const {
        if (start > size()) {
            throw std::out_of_range("bytes_const_view::subspan");
        }
        if (len > size() - start) {
            len = size() - start;
        }
        return {data() + start, len};
    }

    // 隐式转换为 std::string_view
    operator std::string_view() const noexcept {
        return std::string_view{data(), size()};
    }
};

// 表示一个可修改的字节视图，提供对内存中连续字节的读写访问
struct bytes_view {
    // 指向字节数据的指针
    char *m_data;
    // 数据大小
    size_t m_size;

    // 返回数据指针
    char *data() const noexcept {
        return m_data;
    }

    // 返回数据大小
    size_t size() const noexcept {
        return m_size;
    }

    // 返回数据开始的迭代器
    char *begin() const noexcept {
        return data();
    }

    // 返回数据结束的迭代器
    char *end() const noexcept {
        return data() + size();
    }

    // 获取一个子视图，从指定位置开始，长度为 len
    bytes_view subspan(size_t start, size_t len) const {
        if (start > size()) {
            throw std::out_of_range("bytes_view::subspan");
        }
        if (len > size() - start) {
            len = size() - start;
        }
        return {data() + start, len};
    }

    // 隐式转换为 bytes_const_view
    operator bytes_const_view() const noexcept {
        return bytes_const_view{data(), size()};
    }

    // 隐式转换为 std::string_view
    operator std::string_view() const noexcept {
        return std::string_view{data(), size()};
    }
};

// 表示一个动态缓冲区，允许存储和操作字节数据
struct bytes_buffer {
    // 内部数据存储，使用 std::vector 实现
    std::vector<char> m_data;

    // 默认构造函数
    bytes_buffer() = default;
    // 移动构造函数和赋值
    bytes_buffer(bytes_buffer &&) = default;
    bytes_buffer &operator=(bytes_buffer &&) = default;
    // 拷贝构造函数
    explicit bytes_buffer(bytes_buffer const &) = default;

    // 创建指定大小的缓冲区
    explicit bytes_buffer(size_t n) : m_data(n) {}

    // 返回只读数据指针
    char const *data() const noexcept {
        return m_data.data();
    }

    // 返回可写数据指针
    char *data() noexcept {
        return m_data.data();
    }

    // 返回缓冲区大小
    size_t size() const noexcept {
        return m_data.size();
    }

    // 返回开始迭代器
    char const *begin() const noexcept {
        return data();
    }

    char *begin() noexcept {
        return data();
    }

    // 返回结束迭代器
    char const *end() const noexcept {
        return data() + size();
    }

    char *end() noexcept {
        return data() + size();
    }

    // 获取一个只读子视图
    bytes_const_view subspan(size_t start, size_t len) const {
        return operator bytes_const_view().subspan(start, len);
    }

    // 获取一个可写子视图
    bytes_view subspan(size_t start, size_t len) {
        return operator bytes_view().subspan(start, len);
    }

    // 隐式转换为 bytes_const_view
    operator bytes_const_view() const noexcept {
        return bytes_const_view{m_data.data(), m_data.size()};
    }

    // 隐式转换为 bytes_view
    operator bytes_view() noexcept {
        return bytes_view{m_data.data(), m_data.size()};
    }

    // 隐式转换为 std::string_view
    operator std::string_view() const noexcept {
        return std::string_view{m_data.data(), m_data.size()};
    }

    // 添加字节数据
    void append(bytes_const_view chunk) {
        m_data.insert(m_data.end(), chunk.begin(), chunk.end());
    }

    void append(std::string_view chunk) {
        m_data.insert(m_data.end(), chunk.begin(), chunk.end());
    }

    // 添加字面量字符串（不包含空字符）
    template <size_t N>
    void append_literial(char const (&literial)[N]) {
        append(std::string_view{literial, N - 1});
    }

    // 清空缓冲区
    void clear() {
        m_data.clear();
    }

    // 调整缓冲区大小
    void resize(size_t n) {
        m_data.resize(n);
    }

    // 保留缓冲区的最小容量
    void reserve(size_t n) {
        m_data.reserve(n);
    }
};

// 表示一个固定大小的静态缓冲区
template <size_t N>
struct static_bytes_buffer {
    // 固定大小的数据存储
    std::array<char, N> m_data;

    // 返回只读数据指针
    char const *data() const noexcept {
        return m_data.data();
    }

    // 返回可写数据指针
    char *data() noexcept {
        return m_data.data();
    }

    // 返回缓冲区大小
    static constexpr size_t size() noexcept {
        return N;
    }

    // 隐式转换为 bytes_const_view
    operator bytes_const_view() const noexcept {
        return bytes_const_view{m_data.data(), N};
    }

    // 隐式转换为 bytes_view
    operator bytes_view() noexcept {
        return bytes_view{m_data.data(), N};
    }

    // 隐式转换为 std::string_view
    operator std::string_view() const noexcept {
        return std::string_view{m_data.data(), m_data.size()};
    }
};
