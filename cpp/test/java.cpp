#include "ktbind/ktbind.hpp"
#include <cmath>
#include <iostream>
#include <thread>

template <typename L>
std::ostream& write_bracketed_list(std::ostream& os, const L& vec, char left, char right) {
    os << left;
    if (!vec.empty()) {
        auto&& it = vec.begin();
        os << *it;

        for (++it; it != vec.end(); ++it) {
            os << ", " << *it;
        }
    }
    os << right;
    return os;
}

template <typename L>
std::ostream& write_list(std::ostream& os, const L& vec) {
    return write_bracketed_list(os, vec, '[', ']');
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& list) {
    return write_list(os, list);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T>& list) {
    return write_list(os, list);
}

template <typename L>
std::ostream& write_set(std::ostream& os, const L& vec) {
    return write_bracketed_list(os, vec, '{', '}');
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::unordered_set<T>& set) {
    return write_set(os, set);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& set) {
    return write_set(os, set);
}

template <typename M>
std::ostream& write_map(std::ostream& os, const M& map) {
    os << "{";
    if (!map.empty()) {
        auto&& it = map.begin();
        os << it->first << ": " << it->second;

        for (++it; it != map.end(); ++it) {
            os << ", " << it->first << ": " << it->second;
        }
    }
    os << "}";
    return os;
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<K, V>& map) {
    return write_map(os, map);
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& os, const std::unordered_map<K, V>& map) {
    return write_map(os, map);
}

struct Data {
    bool b = true;
    short s = 82;
    int i = 1024;
    long l = 111000111000;
    float f = M_PI;
    double d = M_E;
    std::string str = "sample string in struct";
    std::vector<short> short_arr = { 1,2,3,4,5,6,7,8,9 };
    std::vector<int> int_arr = { 10,20,30,40,50,60,70,80,90 };
    std::vector<long> long_arr = { 100,200,300,400,500,600,700,800,900 };
    std::map<std::string, std::vector<std::string>> map = {
        {"one", {"a","b","c"}},
        {"two", {"x","y","z"}},
        {"three", {}},
        {"four", {"l"}}
    };
};

std::ostream& operator<<(std::ostream& str, const Data& data) {
    return str << "{"
        << "b=" << data.b << ", "
        << "s=" << data.s << ", "
        << "i=" << data.i << ", "
        << "l=" << data.l << ", "
        << "f=" << data.f << ", "
        << "d=" << data.d << ", "
        << "str='" << data.str << "'"
        << "}";
}

struct Sample {
    Sample();
    Sample(const char*);
    Sample(std::string);
    Sample(Sample&&) = default;
    ~Sample();
    Sample duplicate() const;
    Data get_data() const;
    void set_data();
    void set_data(const Data& data);

private:
    Sample(const Sample&) = default;  // ensure that external code cannot make copies

    Data _data;
};

Sample::Sample() {
    JAVA_OUTPUT << "created" << std::endl;
}

Sample::Sample(const char*) {
    JAVA_OUTPUT << "created from const char*" << std::endl;
}

Sample::Sample(std::string) {
    JAVA_OUTPUT << "created from string" << std::endl;
}

Sample::~Sample() {
    JAVA_OUTPUT << "destroyed" << std::endl;
}

Sample Sample::duplicate() const {
    JAVA_OUTPUT << "duplicated" << std::endl;
    return Sample(*this);
}

Data Sample::get_data() const {
    JAVA_OUTPUT << "get nested data" << std::endl;
    return _data;
}

void Sample::set_data() {
    _data = Data();
    JAVA_OUTPUT << "set nested data: " << _data << std::endl;
}

void Sample::set_data(const Data& data) {
    _data = data;
    JAVA_OUTPUT << "set nested data: " << _data << std::endl;
}

void returns_void() {}

bool returns_bool() {
    return true;
}

template <typename T>
T returns_integer() {
    return std::numeric_limits<T>::max();
}

float returns_float() {
    return std::numeric_limits<float>::max();
}

double returns_double() {
    return std::numeric_limits<double>::max();
}

std::string returns_string() {
    return "a sample string";
}

bool pass_arguments_by_value(std::string str, bool b, short s, int i, long l, int16_t i16, int32_t i32, int64_t i64, float f, double d) {
    JAVA_OUTPUT << "("
        << "string = " << str << ", "
        << "bool = " << b << ", "
        << "short = " << s << ", "
        << "int = " << i << ", "
        << "long = " << l << ", "
        << "int16_t = " << i16 << ", "
        << "int32_t = " << i32 << ", "
        << "int64_t = " << i64 << ", "
        << "float = " << f << ", "
        << "double = " << d << ")"
        << std::endl;
    return true;
}

bool pass_arguments_by_reference(const std::string& str, const bool& b, const short& s, const int& i, const long& l, const int16_t& i16, const int32_t& i32, const int64_t& i64, const float& f, const double& d) {
    JAVA_OUTPUT << "("
        << "string = " << str << ", "
        << "bool = " << b << ", "
        << "short = " << s << ", "
        << "int = " << i << ", "
        << "long = " << l << ", "
        << "int16_t = " << i16 << ", "
        << "int32_t = " << i32 << ", "
        << "int64_t = " << i64 << ", "
        << "float = " << f << ", "
        << "double = " << d << ")"
        << std::endl;
    return true;
}

std::vector<int> array_of_int(const std::vector<int>& vec) {
    JAVA_OUTPUT << vec << std::endl;
    return { 0, 1, 2, 3, 4, 5, 6 };
}

std::vector<std::string> array_of_string(const std::vector<std::string>& vec) {
    JAVA_OUTPUT << vec << std::endl;
    return { "", "A", "B", "C", "D", "E", "F" };
}

std::list<int> list_of_int(const std::list<int>& vec) {
    JAVA_OUTPUT << vec << std::endl;
    return { 1, 2, 3, 4, 5, 6 };
}

std::list<std::string> list_of_string(const std::list<std::string>& vec) {
    JAVA_OUTPUT << vec << std::endl;
    return { "A", "B", "C", "D", "E", "F" };
}

std::unordered_set<std::string> unordered_set(std::unordered_set<std::string> s) {
    JAVA_OUTPUT << s << std::endl;
    return { "A", "B", "C" };
}

std::set<std::string> ordered_set(std::set<std::string> s) {
    JAVA_OUTPUT << s << std::endl;
    return { "A", "B", "C" };
}

std::unordered_map<std::string, std::string> unordered_map(std::unordered_map<std::string, std::string> m) {
    JAVA_OUTPUT << m << std::endl;
    return { {"1", "A"}, {"2", "B"}, {"3", "C"} };
}

std::map<long, long> ordered_map_of_int(std::map<long, long> m) {
    JAVA_OUTPUT << m << std::endl;
    return { {1, 1000}, {2, 2000}, {3, 3000} };
}

std::map<std::string, std::string> ordered_map_of_string(std::map<std::string, std::string> m) {
    JAVA_OUTPUT << m << std::endl;
    return { {"1", "A"}, {"2", "B"}, {"3", "C"} };
}

std::map<std::string, std::vector<std::string>> native_composite(std::map<std::string, std::vector<std::string>> m) {
    JAVA_OUTPUT << m << std::endl;
    return { { "A", {"a", "b", "c"} }, { "B", {} }, { "C", {"x"} } };
}

void pass_callback(std::function<void()> fun) {
    fun();
}

std::string pass_callback_returns_string(std::function<std::string()> fun) {
    return fun();
}

int pass_callback_string_returns_int(std::string str, std::function<int(std::string)> fun) {
    return fun(str);
}

std::string pass_callback_string_returns_string(std::string str, std::function<std::string(std::string)> fun) {
    return fun(str);
}

std::string pass_callback_arguments(std::string str, std::function<std::string(std::string, short, int, long)> fun) {
    return fun(str, 4, 82, 112);
}

void callback_on_native_thread(std::function<void()> fun) {
    std::thread([fun]() {
        fun();
    }).join();
}

void raise_native_exception() {
    throw std::runtime_error("an expected error");
}

void catch_java_exception(std::function<void()> fun) {
    try {
        fun();
    } catch (std::exception& ex) {
        JAVA_OUTPUT << "exception caught: " << ex.what() << std::endl;
    }
}

DECLARE_DATA_CLASS(Data, "com.kheiron.ktbind.Data")
DECLARE_NATIVE_CLASS(Sample, "com.kheiron.ktbind.Sample")

JAVA_EXTENSION_MODULE() {
    using namespace java;

    native_class<Sample>()
        .constructor<Sample()>("create")
        .constructor<Sample(std::string)>("create")
        .function<&Sample::duplicate>("duplicate")
        .function<&Sample::get_data>("get_data")
        .function<static_cast<void(Sample::*)(const Data&)>(&Sample::set_data)>("set_data")

        // fundamental types and simple well-known types as return values
        .function<returns_void>("returns_void")
        .function<returns_bool>("returns_bool")
        .function<returns_integer<short>>("returns_short")
        .function<returns_integer<int>>("returns_int")
        .function<returns_integer<long>>("returns_long")
        .function<returns_integer<int16_t>>("returns_int16")
        .function<returns_integer<int32_t>>("returns_int32")
        .function<returns_integer<int64_t>>("returns_int64")
        .function<returns_float>("returns_float")
        .function<returns_double>("returns_double")
        .function<returns_string>("returns_string")

        // passing parameters by value and reference
        .function<pass_arguments_by_value>("pass_arguments_by_value")
        .function<pass_arguments_by_reference>("pass_arguments_by_reference")

        // collections
        .function<array_of_int>("array_of_int")
        .function<array_of_string>("array_of_string")
        .function<list_of_int>("list_of_int")
        .function<list_of_string>("list_of_string")
        .function<unordered_set>("unordered_set")
        .function<ordered_set>("ordered_set")
        .function<unordered_map>("unordered_map")
        .function<ordered_map_of_int>("ordered_map_of_int")
        .function<ordered_map_of_string>("ordered_map_of_string")
        .function<native_composite>("native_composite")

        // callbacks
        .function<pass_callback>("pass_callback")
        .function<pass_callback_returns_string>("pass_callback_returns_string")
        .function<pass_callback_string_returns_int>("pass_callback_string_returns_int")
        .function<pass_callback_string_returns_string>("pass_callback_string_returns_string")
        .function<pass_callback_arguments>("pass_callback_arguments")
        .function<callback_on_native_thread>("callback_on_native_thread")

        // exception handling
        .function<raise_native_exception>("raise_native_exception")
        .function<catch_java_exception>("catch_java_exception")
    ;

    data_class<Data>()
        .field<&Data::b>("b")
        .field<&Data::s>("s")
        .field<&Data::i>("i")
        .field<&Data::l>("l")
        .field<&Data::f>("f")
        .field<&Data::d>("d")
        .field<&Data::str>("str")
        .field<&Data::short_arr>("short_arr")
        .field<&Data::int_arr>("int_arr")
        .field<&Data::long_arr>("long_arr")
        .field<&Data::map>("map")
    ;
    
    print_registered_bindings();
}
