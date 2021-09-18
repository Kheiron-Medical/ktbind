#pragma once

#if __cplusplus < 201703L
#error Including <ktbind.hpp> requires building with -std=c++17 or newer.
#endif

#include <jni.h>
#include <array>
#include <string_view>
#include <string>
#include <sstream>
#include <memory>

#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <algorithm>
#include <functional>
#include <iostream>
#include <cassert>

namespace java {
    /** 
     * Builds a zero-terminated string literal from an std::array.
     * @tparam N The size of the std::array.
     * @tparam I An index sequence, typically constructed with std::make_index_sequence<N>.
     */
    template <std::size_t N, std::array<char, N> const& S, typename I>
    struct to_char_array;

    template <std::size_t N, std::array<char, N> const& S, std::size_t... I>
    struct to_char_array<N, S, std::index_sequence<I...>> {
        static constexpr const char value[] { S[I]..., 0 };
    };

    /**
     * Returns the number of digits in n.
     */ 
    constexpr std::size_t num_digits(std::size_t n) {
        return n < 10 ? 1 : num_digits(n / 10) + 1;
    }

    /**
     * Converts an unsigned integer into sequence of decimal digits.
     */
    template <std::size_t N>
    struct integer_to_digits {
    private:
        constexpr static std::size_t len = num_digits(N);
        constexpr static auto impl() {
            std::array<char, len> arr{};
            std::size_t n = N;
            std::size_t i = len;
            while (i > 0) {
                --i;
                arr[i] = '0' + (n % 10);
                n /= 10;
            }
            return arr;
        }
        constexpr static auto arr = impl();

    public:
        constexpr static std::string_view value = std::string_view(
            to_char_array< arr.size(), arr, std::make_index_sequence<arr.size()> >::value,
            arr.size()
        );
    };

    /**
     * Replaces all occurrences of a character in a string with another character at compile time.
     * @tparam S The string in which replacements are made.
     * @tparam O The character to look for.
     * @tparam N The character to replace to.
     */
    template <std::string_view const& S, char O, char N>
    class replace {
        static constexpr auto impl() noexcept {
            std::array<char, S.size()> arr{};
            for (std::size_t i = 0; i < S.size(); ++i) {
                if (S[i] == O) {
                    arr[i] = N;
                } else {
                    arr[i] = S[i];
                }
            }
            return arr;
        }

        static constexpr auto arr = impl();

    public:
        static constexpr std::string_view value = std::string_view(
            to_char_array< arr.size(), arr, std::make_index_sequence<arr.size()> >::value,
            arr.size()
        );
    };

    template <std::string_view const& S, char O, char N>
    static constexpr auto replace_v = replace<S, O, N>::value;

    /**
     * Concatenates a list of strings at compile time.
     */
    template <std::string_view const&... Strs>
    class join {
        // join all strings into a single std::array of chars
        static constexpr auto impl() noexcept {
            constexpr std::size_t len = (Strs.size() + ... + 0);
            std::array<char, len> arr{};
            auto append = [i = 0, &arr](auto const& s) mutable {
                for (auto c : s) {
                    arr[i++] = c;
                }
            };
            (append(Strs), ...);
            return arr;
        }

        // give the joined string static storage
        static constexpr auto arr = impl();

    public:
        // convert to a string literal, then view as a std::string_view
        static constexpr std::string_view value = std::string_view(
            to_char_array< arr.size(), arr, std::make_index_sequence<arr.size()> >::value,
            arr.size()
        );
    };

    template <std::string_view const&... Strs>
    static constexpr auto join_v = join<Strs...>::value;

    /**
     * Concatenates a list of strings at compile time, inserting a separator between neighboring items.
     */
    template <std::string_view const& Sep, std::string_view const&... Items>
    struct join_sep;
    
    template <std::string_view const& Sep, std::string_view const& Head, std::string_view const&... Tail>
    struct join_sep<Sep, Head, Tail...> {
        constexpr static std::string_view value = join<Head, join<Sep, Tail>::value...>::value;
    };

    template <std::string_view const& Sep, std::string_view const& Item>
    struct join_sep<Sep, Item> {
        constexpr static std::string_view value = Item;
    };

    template <std::string_view const& Sep>
    struct join_sep<Sep> {
        constexpr static std::string_view value = "";
    };

    // helper to extract value
    template <std::string_view const&... items>
    static constexpr auto join_sep_v = join_sep<items...>::value;

    /**
     * Allows a friendly message to be built with the stream insertion operator.
     * 
     * Example: throw std::runtime_error(msg() << "Error: " << code);
     */
    struct msg {
        template<typename T>
        msg& operator<<(const T& part) {
            str << part;
            return *this;
        }

        template<typename T>
        msg& operator<<(T&& part) {
            str << part;
            return *this;
        }

        operator std::string() const {
            return str.str();
        }

    private:
        std::ostringstream str;
    };

    /**
     * An exception that originates from Java.
     */
    struct JavaException : std::exception {
        JavaException(JNIEnv* env) {
            if (env->ExceptionCheck()) {
                ex = env->ExceptionOccurred();

                // clear the exception to allow calling JNI functions
                env->ExceptionClear();

                // extract exception message using low-level functions
                jclass exceptionClass = env->GetObjectClass(ex);
                jmethodID getMessageFunc = env->GetMethodID(exceptionClass, "getMessage", "()Ljava/lang/String;");
                jstring messageObject = static_cast<jstring>(env->CallObjectMethod(ex, getMessageFunc));
                const char* c_str = env->GetStringUTFChars(messageObject, nullptr);
                message = c_str;
                env->ReleaseStringUTFChars(messageObject, c_str);
                env->DeleteLocalRef(messageObject);
                env->DeleteLocalRef(exceptionClass);
            }
        }

        /**
         * Exceptions must not be copied as they contain a JNI local reference.
         */
        JavaException(const JavaException&) = delete;

        const char* what() const noexcept {
            return message.c_str();
        }

        /**
         * Used by the interoperability framework to re-throw the exception in Java before crossing the native to Java
         * boundary, unless the exception has been caught by the user.
         */
        jthrowable innerException() const noexcept {
            return ex;
        }

    private:
        jthrowable ex = nullptr;
        std::string message;
    };

    class LocalClassRef;

    /**
     * C++ wrapper class of [jmethodID] for instance methods.
     */
    class Method {
        Method(JNIEnv* env, jclass cls, const char* name, const std::string_view& signature) {
            _ref = env->GetMethodID(cls, name, signature.data());
            if (_ref == nullptr) {
                throw JavaException(env);  // method not found
            }
        }

        friend LocalClassRef; 

        jmethodID _ref = nullptr;

    public:
        Method() = default;

        jmethodID ref() const {
            return _ref;
        }
    };

    /**
     * C++ wrapper class of [jmethodID] for class methods.
     */
    class StaticMethod {
        StaticMethod(JNIEnv* env, jclass cls, const char* name, const std::string_view& signature) {
            _ref = env->GetStaticMethodID(cls, name, signature.data());
            if (_ref == nullptr) {
                throw JavaException(env);  // method not found
            }
        }

        friend LocalClassRef; 

        jmethodID _ref = nullptr;

    public:
        StaticMethod() = default;

        jmethodID ref() const {
            return _ref;
        }
    };

    /**
     * C++ wrapper class of [jfieldID] for instance fields.
     */
    class Field {
        Field(JNIEnv* env, jclass cls, const char* name, const std::string_view& signature) {
            _ref = env->GetFieldID(cls, name, signature.data());
            if (_ref == nullptr) {
                throw JavaException(env);  // field not found
            }
        }

        friend LocalClassRef;

        jfieldID _ref = nullptr;

    public:
        Field() = default;

        jfieldID ref() const {
            return _ref;
        }
    };

    /**
     * C++ wrapper class of [jfieldID] for class fields.
     */
    class StaticField {
        StaticField(JNIEnv* env, jclass cls, const char* name, const std::string_view& signature) {
            _ref = env->GetStaticFieldID(cls, name, signature.data());
            if (_ref == nullptr) {
                throw JavaException(env);  // field not found
            }
        }

        friend LocalClassRef;

        jfieldID _ref = nullptr;

    public:
        StaticField() = default;

        jfieldID ref() const {
            return _ref;
        }
    };

    /**
     * Scoped C++ wrapper class of a [jobject] that is used only within a single native execution block.
     */
    class LocalObjectRef {
    public:
        LocalObjectRef() = default;
        LocalObjectRef(JNIEnv* env, jobject obj) : _env(env), _ref(obj) {}

        LocalObjectRef(const LocalObjectRef& op) = delete;
        LocalObjectRef& operator=(const LocalObjectRef& op) = delete;

        LocalObjectRef(LocalObjectRef&& op) : _env(op._env), _ref(op._ref) {
            op._ref = nullptr;
        }

        ~LocalObjectRef() {
            if (_ref != nullptr) {
                _env->DeleteLocalRef(_ref);
            }
        }

        jobject ref() const {
            return _ref;
        }

    private:
        JNIEnv* _env = nullptr;
        jobject _ref = nullptr;
    };

    /**
     * Scoped C++ wrapper class of [jclass].
     */
    class LocalClassRef {
    public:
        LocalClassRef(JNIEnv* env, const char* name) : LocalClassRef(env, name, std::nothrow) {
            if (_ref == nullptr) {
                throw JavaException(env);  // Java class not found
            }
        }

        LocalClassRef(JNIEnv* env, const char* name, std::nothrow_t) : _env(env) {
            _ref = env->FindClass(name);
        }

        LocalClassRef(JNIEnv* env, jobject obj) : _env(env) {
            _ref = env->GetObjectClass(obj);
        }

        LocalClassRef(JNIEnv* env, jclass cls) : _env(env), _ref(cls) {}

        ~LocalClassRef() {
            if (_ref != nullptr) {
                _env->DeleteLocalRef(_ref);
            }
        }

        LocalClassRef(const LocalClassRef&) = delete;
        LocalClassRef& operator=(const LocalClassRef& op) = delete;

        LocalClassRef(LocalClassRef&& op) : _env(op._env), _ref(op._ref) {
            op._ref = nullptr;
        }

        Method getMethod(const char* name, const std::string_view& signature) {
            return Method(_env, _ref, name, signature);
        }

        Field getField(const char* name, const std::string_view& signature) {
            return Field(_env, _ref, name, signature);
        }

        StaticMethod getStaticMethod(const char* name, const std::string_view& signature) {
            return StaticMethod(_env, _ref, name, signature);
        }

        StaticField getStaticField(const char* name, const std::string_view& signature) {
            return StaticField(_env, _ref, name, signature);
        }

        LocalObjectRef getStaticObjectField(const char* name, const std::string_view& signature) {
            StaticField fld = getStaticField(name, signature);
            return LocalObjectRef(_env, _env->GetStaticObjectField(_ref, fld.ref()));
        }

        jclass ref() const {
            return _ref;
        }

    private:
        JNIEnv* _env; 
        jclass _ref;
    };

    /**
     * Represents the JNI environment in which the extension module is executing.
     */
    struct Environment {
        /** Triggered by the function `JNI_OnLoad`. */
        static void load(JavaVM* vm) {
            assert(_vm == nullptr);
            _vm = vm;
        }

        /** Triggered by the function `JNI_OnUnload`. */
        static void unload(JavaVM* vm) {
            assert(_vm != nullptr);
            _vm = nullptr;
        }

        void setEnv(JNIEnv* env) {
            assert(_vm != nullptr);
            assert(_env == nullptr || _env == env);
            _env = env;
        }

        JNIEnv* getEnv() {
            assert(_vm != nullptr);
            
            if (_env == nullptr) {
                // attach thread to obtain an environment
                switch (_vm->GetEnv(reinterpret_cast<void**>(&_env), JNI_VERSION_1_6)) {
                    case JNI_OK:
                        break;
                    case JNI_EDETACHED:
                        if (_vm->AttachCurrentThread(reinterpret_cast<void**>(&_env), nullptr) == JNI_OK) {
                            assert(_env != nullptr);
                            _attached = true;
                        } else {
                            // failed to attach thread
                            return nullptr;
                        }
                        break;
                    case JNI_EVERSION:
                    default:
                        // unsupported JVM version or other error
                        return nullptr;
                }
            }

            return _env;
        }

        ~Environment() {
            if (!_env) {
                return;
            }
            
            // only threads explicitly attached by native code should be released
            if (_vm != nullptr && _attached) {
                // detach thread
                _vm->DetachCurrentThread();
            }
        }

    private:
        inline static JavaVM* _vm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    /**
     * Ensures that Java resources allocated by the thread are released when the thread terminates.
     */
    static thread_local Environment this_thread;

    /**
     * An adapter for an object reference handle that remains valid as the native-to-Java boundary is crossed.
     */
    class GlobalObjectRef {
    public:
        using jobject_struct = std::pointer_traits<jobject>::element_type;

        GlobalObjectRef(JNIEnv* env, jobject obj) {
            _ref = std::shared_ptr<jobject_struct>(env->NewGlobalRef(obj), [](jobject ref) {
                JNIEnv* env = this_thread.getEnv();
                if (env != nullptr) {
                    env->DeleteGlobalRef(ref);
                }
            });
        }

        jobject ref() const {
            return _ref.get();
        }

    private:
        std::shared_ptr<jobject_struct> _ref;
    };

    /**
     * Used in static_assert to have the type name printed in the compiler error message.
     */
    template <typename>
    struct fail : std::false_type {};

    /**
     * Argument type traits, and argument type conversion between native and Java.
     * 
     * Template substitution fails automatically unless the type is a well-known type or has been declared
     * with DECLARE_DATA_CLASS or DECLARE_NATIVE_CLASS.
     */
    template <typename T>
    struct ArgType {
        using native_type = void;
        using java_type = void;

        // unused, included to suppress compiler error messages
        constexpr static std::string_view kotlin_type = "";
        constexpr static std::string_view type_sig = "";

        // returns the corresponding array type for the base type
        constexpr static std::string_view array_type_prefix = "[";
        constexpr static std::string_view array_type_sig = join_v<array_type_prefix, ArgType<T>::type_sig>;

        // unused, included to suppress compiler error messages
        static void native_value(JNIEnv*, jobject value) {}
        static void java_value(JNIEnv* env, const T& value) {}

        static_assert(fail<T>::value, "Unrecognized type detected, ensure that all involved types have been declared as a binding type with DECLARE_DATA_CLASS or DECLARE_NATIVE_CLASS.");
    };

    /**
     * Base class for converting primitive types such as [int] or [double].
     */
    template <typename T, typename J>
    struct FundamentalArgType {
        using native_type = T;
        using java_type = J;

    private:
        constexpr static std::string_view lparen = "(";
        constexpr static std::string_view rparen = ")";
        constexpr static std::string_view class_type_prefix = "L";
        constexpr static std::string_view class_type_suffix = ";";

        /** Used in looking up the appropriate `valueOf()` function to instantiate object wrappers of values of a primitive type. */
        constexpr static std::string_view value_initializer = join_v<lparen, ArgType<T>::type_sig, rparen, class_type_prefix, ArgType<T>::class_name, class_type_suffix>;

        /** Used in looking up the appropriate `primitiveValue()` function to fetch the primitive type (e.g. `intValue()` for `int`). */
        constexpr static std::string_view get_value_func_suffix = "Value";
        constexpr static std::string_view get_value_func = join_v<ArgType<T>::primitive_type, get_value_func_suffix>;
        constexpr static std::string_view get_value_func_sig = join_v<lparen, rparen, ArgType<T>::type_sig>;

    public:
        static T native_value(JNIEnv*, J value) {
            return static_cast<T>(value);
        }

        static J java_value(JNIEnv* env, T value) {
            return static_cast<J>(value);
        }

        /**
         * Wraps the primitive type (e.g. int) into an object type (e.g. Integer).
         */
        static jobject java_box(JNIEnv* env, J value) {
            LocalClassRef cls(env, ArgType<T>::class_name.data());
            StaticMethod valueOf = cls.getStaticMethod("valueOf", value_initializer);
            return env->CallStaticObjectMethod(cls.ref(), valueOf.ref(), value);
        }

        /**
         * Unwraps a primitive type (e.g. int) from an object type (e.g. Integer).
         */
        static J java_unbox(JNIEnv* env, jobject obj) {
            LocalClassRef cls(env, obj);
            Method getValue = cls.getMethod(get_value_func.data(), get_value_func_sig);
            return ArgType<T>::java_call_method(env, obj, getValue);
        }

        /**
         * Extracts a native value from a Java object field.
         */
        static T native_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return ArgType<T>::native_value(env, ArgType<T>::java_raw_field_value(env, obj, fld));
        }

        /**
         * Creates a Java array from a range of native values.
         * To be implemented by concrete types.
         */
        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len);
    };

    template <>
    struct ArgType<void> {
        constexpr static std::string_view kotlin_type = "Unit";
        constexpr static std::string_view type_sig = "V";
        using native_type = void;
        using java_type = void;
    };

    template <>
    struct ArgType<bool> : FundamentalArgType<bool, jboolean> {
        constexpr static std::string_view class_name = "java/lang/Boolean";
        constexpr static std::string_view primitive_type = "boolean";
        constexpr static std::string_view kotlin_type = "Boolean";
        constexpr static std::string_view type_sig = "Z";

        static jboolean java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallBooleanMethod(obj, m.ref());
        }

        static jboolean java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetBooleanField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            env->SetBooleanField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, native_type* ptr, std::size_t len) {
            static_assert(sizeof(native_type) == sizeof(java_type), "C++ boolean type and JNI jboolean type are expected to match in size.");
            env->GetBooleanArrayRegion(static_cast<jbooleanArray>(arr), 0, len, reinterpret_cast<jboolean*>(ptr));
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            static_assert(sizeof(native_type) == sizeof(java_type), "C++ boolean type and JNI jboolean type are expected to match in size.");
            jbooleanArray arr = env->NewBooleanArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetBooleanArrayRegion(arr, 0, len, reinterpret_cast<const jboolean*>(ptr));
            return arr;
        }
    };

    template <typename T>
    struct CharArgType : FundamentalArgType<T, jbyte> {
        constexpr static std::string_view class_name = "java/lang/Byte";
        constexpr static std::string_view primitive_type = "byte";
        constexpr static std::string_view kotlin_type = "Byte";
        constexpr static std::string_view type_sig = "B";

        static jbyte java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallByteMethod(obj, m.ref());
        }

        static jbyte java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetByteField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, T value) {
            env->SetByteField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, T* ptr, std::size_t len) {
            env->GetByteArrayRegion(static_cast<jbyteArray>(arr), 0, len, reinterpret_cast<jbyte*>(ptr));
        }

        static jarray java_array_value(JNIEnv* env, const T* ptr, std::size_t len) {
            jbyteArray arr = env->NewByteArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetByteArrayRegion(arr, 0, len, reinterpret_cast<const jbyte*>(ptr));
            return arr;
        }
    };

    template <> struct ArgType<char> : CharArgType<char> {};
    template <> struct ArgType<signed char> : CharArgType<signed char> {};
    template <> struct ArgType<unsigned char> : CharArgType<unsigned char> {};

    template <>
    struct ArgType<uint16_t> : FundamentalArgType<uint16_t, jchar> {
        constexpr static std::string_view class_name = "java/lang/Character";
        constexpr static std::string_view primitive_type = "char";
        constexpr static std::string_view kotlin_type = "Char";
        constexpr static std::string_view type_sig = "C";

        static jchar java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallCharMethod(obj, m.ref());
        }

        static jchar java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetCharField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            env->SetCharField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, native_type* ptr, std::size_t len) {
            env->GetCharArrayRegion(static_cast<jcharArray>(arr), 0, len, ptr);
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            jcharArray arr = env->NewCharArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetCharArrayRegion(arr, 0, len, ptr);
            return arr;
        }
    };

    template <>
    struct ArgType<short> : FundamentalArgType<short, jshort> {
        constexpr static std::string_view class_name = "java/lang/Short";
        constexpr static std::string_view primitive_type = "short";
        constexpr static std::string_view kotlin_type = "Short";
        constexpr static std::string_view type_sig = "S";

        static jshort java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallShortMethod(obj, m.ref());
        }

        static jshort java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetShortField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            env->SetShortField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, native_type* ptr, std::size_t len) {
            env->GetShortArrayRegion(static_cast<jshortArray>(arr), 0, len, ptr);
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            jshortArray arr = env->NewShortArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetShortArrayRegion(arr, 0, len, ptr);
            return arr;
        }
    };

    template <typename T>
    struct Int32ArgType : FundamentalArgType<T, jint> {
        static_assert(sizeof(T) == 4, "32-bit integer type required.");

        constexpr static std::string_view class_name = "java/lang/Integer";
        constexpr static std::string_view primitive_type = "int";
        constexpr static std::string_view kotlin_type = "Int";
        constexpr static std::string_view type_sig = "I";

        static jint java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallIntMethod(obj, m.ref());
        }

        static jint java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetIntField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, T value) {
            env->SetIntField(obj, fld.ref(), ArgType<T>::java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, T* ptr, std::size_t len) {
            static_assert(sizeof(T) == sizeof(jint), "C++ and JNI integer types are expected to match in size.");
            env->GetIntArrayRegion(static_cast<jintArray>(arr), 0, len, reinterpret_cast<jint*>(ptr));
        }

        static jarray java_array_value(JNIEnv* env, const T* ptr, std::size_t len) {
            static_assert(sizeof(T) == sizeof(jint), "C++ and JNI integer types are expected to match in size.");
            jintArray arr = env->NewIntArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetIntArrayRegion(arr, 0, len, reinterpret_cast<const jint*>(ptr));
            return arr;
        }
    };

    template <typename T>
    struct Int64ArgType : FundamentalArgType<T, jlong> {
        static_assert(sizeof(T) == 8, "64-bit integer type required.");

        constexpr static std::string_view class_name = "java/lang/Long";
        constexpr static std::string_view primitive_type = "long";
        constexpr static std::string_view kotlin_type = "Long";
        constexpr static std::string_view type_sig = "J";

        static jlong java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallLongMethod(obj, m.ref());
        }

        static jlong java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetLongField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, T value) {
            env->SetLongField(obj, fld.ref(), ArgType<T>::java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, T* ptr, std::size_t len) {
            static_assert(sizeof(T) == sizeof(jlong), "C++ and JNI integer types are expected to match in size.");
            env->GetLongArrayRegion(static_cast<jlongArray>(arr), 0, len, reinterpret_cast<jlong*>(ptr));
        }

        static jarray java_array_value(JNIEnv* env, const T* ptr, std::size_t len) {
            static_assert(sizeof(T) == sizeof(jlong), "C++ and JNI integer types are expected to match in size.");
            jlongArray arr = env->NewLongArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetLongArrayRegion(arr, 0, len, reinterpret_cast<const jlong*>(ptr));
            return arr;
        }
    };

    template <typename T>
    struct IntegerArgType : std::conditional<sizeof(T) <= sizeof(int32_t), Int32ArgType<T>, Int64ArgType<T>>::type {};

    template <> struct ArgType<int> : IntegerArgType<int> {};
    template <> struct ArgType<long> : IntegerArgType<long> {};
    template <> struct ArgType<long long> : IntegerArgType<long long> {};
    template <> struct ArgType<unsigned int> : IntegerArgType<unsigned int> {};
    template <> struct ArgType<unsigned long> : IntegerArgType<unsigned long> {};
    template <> struct ArgType<unsigned long long> : IntegerArgType<unsigned long long> {};

    /**
     * Specialized type for storing native pointers in Java.
     * Reserved for use by the interoperability framework.
     */
    template <typename T>
    struct ArgType<T*> : private FundamentalArgType<std::intptr_t, jlong> {
        constexpr static std::string_view class_name = "java/lang/Long";
        constexpr static std::string_view kotlin_type = "Long";
        constexpr static std::string_view type_sig = "J";

        static T* native_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return reinterpret_cast<T*>(native_value(env, env->GetLongField(obj, fld.ref())));
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, T* value) {
            env->SetLongField(obj, fld.ref(), java_value(env, reinterpret_cast<intptr_t>(value)));
        }
    };

    template <>
    struct ArgType<float> : FundamentalArgType<float, jfloat> {
        constexpr static std::string_view class_name = "java/lang/Float";
        constexpr static std::string_view primitive_type = "float";
        constexpr static std::string_view kotlin_type = "Float";
        constexpr static std::string_view type_sig = "F";

        static jfloat java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallFloatMethod(obj, m.ref());
        }

        static jfloat java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetFloatField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            env->SetFloatField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, native_type* ptr, std::size_t len) {
            env->GetFloatArrayRegion(static_cast<jfloatArray>(arr), 0, len, ptr);
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            jfloatArray arr = env->NewFloatArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetFloatArrayRegion(arr, 0, len, ptr);
            return arr;
        }
    };

    template <>
    struct ArgType<double> : FundamentalArgType<double, jdouble> {
        constexpr static std::string_view class_name = "java/lang/Double";
        constexpr static std::string_view primitive_type = "double";
        constexpr static std::string_view kotlin_type = "Double";
        constexpr static std::string_view type_sig = "D";

        static jdouble java_call_method(JNIEnv* env, jobject obj, Method& m) {
            return env->CallDoubleMethod(obj, m.ref());
        }

        static jdouble java_raw_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return env->GetDoubleField(obj, fld.ref());
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            env->SetDoubleField(obj, fld.ref(), java_value(env, value));
        }

        static void native_array_value(JNIEnv* env, jarray arr, native_type* ptr, std::size_t len) {
            env->GetDoubleArrayRegion(static_cast<jdoubleArray>(arr), 0, len, ptr);
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            jdoubleArray arr = env->NewDoubleArray(len);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            env->SetDoubleArrayRegion(arr, 0, len, ptr);
            return arr;
        }
    };

    /**
     * Generates a human-readable name for specialized generic types, e.g. List<String>.
     */
    template <std::string_view const& template_type, std::string_view const&... arg_types>
    struct kotlin_type_specialization {
        constexpr static std::string_view comma = ", ";
        constexpr static std::string_view lt = "<";
        constexpr static std::string_view gt = ">";
        constexpr static std::string_view value = join_v<template_type, lt, join_sep_v<comma, arg_types...>, gt>;
    };

    /**
     * Reserved type tag to represent Java arrays.
     */
    template <typename T>
    struct Array;

    template <typename T>
    struct ArgType<Array<T>> {
    private:
        constexpr static std::string_view array_type_prefix = "[";
        constexpr static std::string_view array_type = "Array";

    public:
        constexpr static std::string_view kotlin_type = kotlin_type_specialization<array_type, ArgType<T>::kotlin_type>::value;
        constexpr static std::string_view type_sig = join_v<array_type_prefix, ArgType<T>::type_sig>;
        using native_type = Array<T>;
        using java_type = jarray;
    };

    /**
     * Base class for all object types such as String, List<T>, Map<K, V>, data classes and native classes.
     */
    template <typename T, typename J>
    struct CompositeArgType {
        using native_type = T;
        using java_type = J;

    private:
        constexpr static std::string_view class_type_prefix = "L";
        constexpr static std::string_view class_type_suffix = ";";

    public:
        constexpr static std::string_view kotlin_type = ArgType<T>::qualified_name;
        constexpr static std::string_view type_sig = join_v<class_type_prefix, ArgType<T>::class_name, class_type_suffix>;

        static native_type native_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return ArgType<T>::native_value(env, env->GetObjectField(obj, fld.ref()));
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, native_type value) {
            // use local reference to ensure temporary object is released
            LocalObjectRef objFieldValue(env, ArgType<T>::java_value(env, value));
            env->SetObjectField(obj, fld.ref(), objFieldValue.ref());
        }

        static jobject java_box(JNIEnv* env, J value) {
            return value;
        }

        static J java_unbox(JNIEnv* env, jobject obj) {
            return obj;
        }
    };

    /**
     * Reserved type tag to represent Java objects.
     */
    struct Object;

    template <>
    struct ArgType<Object> : CompositeArgType<Object, jobject> {
        constexpr static std::string_view qualified_name = "java.lang.Object";
        constexpr static std::string_view class_name = "java/lang/Object";
    };

    /**
     * Converts a C++ string (represented in UTF-8) into a java.lang.String.
     */
    template <>
    struct ArgType<std::string> : CompositeArgType<std::string, jstring> {
        constexpr static std::string_view qualified_name = "java.lang.String";
        constexpr static std::string_view class_name = "java/lang/String";

        static jstring java_unbox(JNIEnv* env, jobject obj) {
            return static_cast<jstring>(obj);  // only a pointer cast
        }

        static native_type native_field_value(JNIEnv* env, jobject obj, Field& fld) {
            return native_value(env, static_cast<jstring>(env->GetObjectField(obj, fld.ref())));
        }

        static std::string native_value(JNIEnv* env, jstring value) {
            jsize len = env->GetStringUTFLength(value);
            std::string s;
            if (len > 0) {
                const char* chars = env->GetStringUTFChars(value, nullptr);
                s.assign(chars, len);
                env->ReleaseStringUTFChars(value, chars);
            }
            return s;
        }

        static jstring java_value(JNIEnv* env, const std::string& value) {
            return env->NewStringUTF(value.data());
        }
    };

    /**
     * Converts a C++ collection with a forward iterator into a Java List.
     */
    template <typename L, typename T>
    struct ListArgType : CompositeArgType<L, jobject> {
        constexpr static std::string_view qualified_name = "java.util.List";
        constexpr static std::string_view class_name = "java/util/List";
        constexpr static std::string_view kotlin_type = kotlin_type_specialization<qualified_name, ArgType<T>::kotlin_type>::value;

    public:
        static L native_value(JNIEnv* env, jobject list);
        static jobject java_value(JNIEnv* env, const L& value);
    };

    template <typename T>
    struct FundamentalArgArrayType {
        using native_type = std::vector<T>;
        using java_type = jarray;

    private:
        constexpr static std::string_view kotlin_array = "Array";
        constexpr static std::string_view array_type_prefix = "[";

    public:
        constexpr static std::string_view kotlin_type = join_v<ArgType<T>::kotlin_type, kotlin_array>;
        constexpr static std::string_view type_sig = join_v<array_type_prefix, ArgType<T>::type_sig>;

        static native_type native_field_value(JNIEnv* env, jobject obj, Field& fld) {
            LocalObjectRef objFieldValue(env, env->GetObjectField(obj, fld.ref()));
            return native_value(env, static_cast<jarray>(objFieldValue.ref()));
        }

        static void java_field_value(JNIEnv* env, jobject obj, Field& fld, const native_type& value) {
            LocalObjectRef objFieldValue(env, java_value(env, value));
            env->SetObjectField(obj, fld.ref(), objFieldValue.ref());
        }

        static jarray java_box(JNIEnv* env, jarray value) {
            return value;
        }

        static jarray java_unbox(JNIEnv* env, jarray obj) {
            return obj;
        }

        static native_type native_value(JNIEnv* env, jarray arr) {
            std::size_t len = env->GetArrayLength(arr);
            native_type vec(len);
            ArgType<T>::native_array_value(env, arr, vec.data(), vec.size());
            return vec;
        }

        static jarray java_value(JNIEnv* env, const native_type& arr) {
            return ArgType<T>::java_array_value(env, arr.data(), arr.size());
        }
    };

    /**
     * Converts a native dynamically-sized array into a Java List.
     */
    template <typename T>
    struct ArgType<std::vector<T>> : ListArgType<std::vector<T>, T> {};

    template <> struct ArgType<std::vector<char>> : FundamentalArgArrayType<char> {};
    template <> struct ArgType<std::vector<signed char>> : FundamentalArgArrayType<signed char> {};
    template <> struct ArgType<std::vector<unsigned char>> : FundamentalArgArrayType<unsigned char> {};
    template <> struct ArgType<std::vector<uint16_t>> : FundamentalArgArrayType<uint16_t> {};
    template <> struct ArgType<std::vector<short>> : FundamentalArgArrayType<short> {};
    template <> struct ArgType<std::vector<int>> : FundamentalArgArrayType<int> {};
    template <> struct ArgType<std::vector<long>> : FundamentalArgArrayType<long> {};
    template <> struct ArgType<std::vector<long long>> : FundamentalArgArrayType<long long> {};
    template <> struct ArgType<std::vector<unsigned int>> : FundamentalArgArrayType<unsigned int> {};
    template <> struct ArgType<std::vector<unsigned long>> : FundamentalArgArrayType<unsigned long> {};
    template <> struct ArgType<std::vector<unsigned long long>> : FundamentalArgArrayType<unsigned long long> {};
    template <> struct ArgType<std::vector<float>> : FundamentalArgArrayType<float> {};
    template <> struct ArgType<std::vector<double>> : FundamentalArgArrayType<double> {};

    template <typename T>
    struct ArgType<std::list<T>> : ListArgType<std::list<T>, T> {};

    /**
     * Converts a native set (e.g. a [set] or [unordered_set]) into a Java Set.
     */
    template <typename S, typename E>
    struct SetArgType : CompositeArgType<S, jobject> {
        constexpr static std::string_view qualified_name = "java.util.Set";
        constexpr static std::string_view class_name = "java/util/Set";
        constexpr static std::string_view kotlin_type = kotlin_type_specialization<qualified_name, ArgType<E>::kotlin_type>::value;

        static S native_value(JNIEnv* env, jobject set);
        static jobject java_value(JNIEnv* env, const S& nativeSet);
    };

    template <typename E>
    struct ArgType<std::unordered_set<E>> : SetArgType<std::unordered_set<E>, E> {
        constexpr static std::string_view concrete_class_name = "java/util/HashSet";
    };

    template <typename E>
    struct ArgType<std::set<E>> : SetArgType<std::set<E>, E> {
        constexpr static std::string_view concrete_class_name = "java/util/TreeSet";
    };

    /**
     * Converts a native dictionary (e.g. a [map] or [unordered_map]) into a Java Map.
     */
    template <typename M, typename K, typename V>
    struct MapArgType : CompositeArgType<M, jobject> {
        constexpr static std::string_view qualified_name = "java.util.Map";
        constexpr static std::string_view class_name = "java/util/Map";
        constexpr static std::string_view kotlin_type = kotlin_type_specialization<qualified_name, ArgType<K>::kotlin_type, ArgType<V>::kotlin_type>::value;

        static M native_value(JNIEnv* env, jobject map);
        static jobject java_value(JNIEnv* env, const M& nativeMap);
    };

    template <typename K, typename V>
    struct ArgType<std::unordered_map<K, V>> : MapArgType<std::unordered_map<K, V>, K, V> {
        constexpr static std::string_view concrete_class_name = "java/util/HashMap";
    };

    template <typename K, typename V>
    struct ArgType<std::map<K, V>> : MapArgType<std::map<K, V>, K, V> {
        constexpr static std::string_view concrete_class_name = "java/util/TreeMap";
    };

    /**
     * Meta-information about a native class member variable.
     */
    struct FieldBinding {
        /** The field name as it appears in the class definition. */
        std::string_view name;
        /** The Java type signature associated with the field */
        std::string_view signature;
        /** A function that extracts a value from a Java object field. */
        void (*get_by_value)(JNIEnv* env, jobject obj, Field& fld, const void* native_object_ptr);
        /** A function that persists a value to a Java object field. */
        void (*set_by_value)(JNIEnv* env, jobject obj, Field& fld, void* native_object_ptr);
    };

    /**
     * Stores meta-information about the member variables a native class type has.
     */
    struct FieldBindings {
        inline static std::map< std::string_view, std::vector<FieldBinding> > value;
    };

    /**
     * Marshals types that are passed by value between C++ and Java/Kotlin.
     */
    template <typename T>
    struct DataClassArgType : CompositeArgType<T, jobject> {
        using native_type = T;
        
        constexpr static std::string_view type_sig = CompositeArgType<T, jobject>::type_sig;

        static T native_value(JNIEnv* env, jobject obj) {
            LocalClassRef objClass(env, obj);
            
            T native_object;
            auto&& bindings = FieldBindings::value[type_sig];
            for (auto&& binding : bindings) {
                Field fld = objClass.getField(binding.name.data(), binding.signature);
                binding.set_by_value(env, obj, fld, &native_object);
            }
            return native_object;
        }

        static jobject java_value(JNIEnv* env, const T& native_object) {
            LocalClassRef objClass(env, type_sig.data());
            jobject obj = env->AllocObject(objClass.ref());
            if (obj == nullptr) {
                throw JavaException(env);
            }
            
            auto&& bindings = FieldBindings::value[type_sig];
            for (auto&& binding : bindings) {
                Field fld = objClass.getField(binding.name.data(), binding.signature);
                binding.get_by_value(env, obj, fld, &native_object);
            }
            return obj;
        }

        static jarray java_array_value(JNIEnv* env, const native_type* ptr, std::size_t len) {
            LocalClassRef elementClass(env, ArgType<T>::class_name.data());
            jobjectArray arr = env->NewObjectArray(len, elementClass.ref(), nullptr);
            if (arr == nullptr) {
                throw JavaException(env);
            }
            for (std::size_t k = 0; k < len; ++k) {
                LocalObjectRef objElement(env, ArgType<T>::java_value(env, ptr[k]));
                env->SetObjectArrayElement(arr, k, objElement.ref());
            }
            return arr;
        }
    };

    /**
     * Marshals types that live primarily in the native space, and accessed through an opaque handle in Java/Kotlin.
     */
    template <typename T>
    struct NativeClassArgType : CompositeArgType<T, jobject> {
        using native_type = T;
        
        constexpr static std::string_view type_sig = CompositeArgType<T, jobject>::type_sig;

        static T& native_value(JNIEnv* env, jobject obj) {
            // look up field that stores native pointer
            LocalClassRef cls(env, obj);
            Field field = cls.getField("nativePointer", ArgType<T*>::type_sig.data());
            T* ptr = ArgType<T*>::native_field_value(env, obj, field);
            return *ptr;
        }

        static jobject java_value(JNIEnv* env, T&& native_object) {
            // instantiate native object using copy or move constructor
            T* ptr = new T(std::forward<T>(native_object));

            // instantiate Java object by skipping constructor
            LocalClassRef objClass(env, ArgType<T>::class_name.data());
            jobject obj = env->AllocObject(objClass.ref());
            if (obj == nullptr) {
                throw JavaException(env);
            }

            // store native pointer in Java object field
            Field field = objClass.getField("nativePointer", ArgType<T*>::type_sig.data());
            ArgType<T*>::java_field_value(env, obj, field, ptr);

            return obj;
        }
    };

    template <typename>
    struct FieldType;

    template <typename T, typename R>
    struct FieldType<R(T::*)> {
        using type = R;
    };

    template <typename>
    struct Function;

    /**
     * Extracts a Java signature from a native callable.
     */
    template <typename R, typename... Args>
    struct Function<R(Args...)> {
    private:
        static_assert(std::is_same<R, typename ArgType<R>::native_type>::value, "Unrecognized type detected, ensure that all involved types have been declared as a binding type with DECLARE_DATA_CLASS or DECLARE_NATIVE_CLASS.");
        static_assert((std::is_same<Args, typename ArgType<Args>::native_type>::value && ...), "Unrecognized type detected, ensure that all involved types have been declared as a binding type with DECLARE_DATA_CLASS or DECLARE_NATIVE_CLASS.");

        template <std::string_view const& param_sig, std::string_view const& return_sig>
        struct callable_sig {
            constexpr static std::string_view lparen = "(";
            constexpr static std::string_view rparen = ")";
            constexpr static std::string_view value = join_v<lparen, param_sig, rparen, return_sig>;
        };

        template <typename I>
        struct function_params;

        template <std::size_t... I>
        struct function_params<std::index_sequence<I...>> {
            constexpr static std::string_view arg = "arg";
            constexpr static std::string_view comma = ", ";
            constexpr static std::string_view colon = ": ";
            constexpr static std::string_view value = join_sep_v<comma, join_v<arg, integer_to_digits<I>::value, colon, ArgType<Args>::kotlin_type>...>;
        };

        struct lambda_params {
            constexpr static std::string_view comma = ", ";
            constexpr static std::string_view value = join_sep_v<comma, ArgType<Args>::kotlin_type...>;
        };

        constexpr static std::string_view param_sig = join_v<ArgType<Args>::type_sig...>;
        constexpr static std::string_view return_sig = ArgType<R>::type_sig;

        constexpr static std::string_view colon = ": ";
        constexpr static std::string_view kotlin_params = function_params<std::index_sequence_for<Args...>>::value;
        constexpr static std::string_view kotlin_return = join_v<colon, ArgType<R>::kotlin_type>;

        constexpr static std::string_view arrow = " -> ";
        constexpr static std::string_view kotlin_lambda_params = lambda_params::value;
        constexpr static std::string_view kotlin_lambda_return = join_v<arrow, ArgType<R>::kotlin_type>;

    public:
        /** Java signature string used internally for type lookup. */
        constexpr static std::string_view signature = callable_sig<param_sig, return_sig>::value;
        
        /** Human-readable type definition for use as a member function. */
        constexpr static std::string_view kotlin_type = callable_sig<kotlin_params, kotlin_return>::value;

        /** Human-readable type definition for use as a lambda type. */
        constexpr static std::string_view kotlin_lambda_type = callable_sig<kotlin_params, kotlin_lambda_return>::value;
    };

    /**
     * Extracts a Java signature from a native free (non-member) function.
     */
    template <typename R, typename... Args>
    struct Function<R(*)(Args...)> {
        constexpr static std::string_view signature = Function<R(std::decay_t<Args>...)>::signature;
        constexpr static std::string_view kotlin_type = Function<R(std::decay_t<Args>...)>::kotlin_type;
    };

    /**
     * Extracts a Java signature from a native member function.
     */
    template <typename T, typename R, typename... Args>
    struct Function<R(T::*)(Args...)> {
        constexpr static std::string_view signature = Function<R(std::decay_t<Args>...)>::signature;
        constexpr static std::string_view kotlin_type = Function<R(std::decay_t<Args>...)>::kotlin_type;
    };

    /**
     * Extracts a Java signature from a const-qualified native member function.
     */
    template <typename T, typename R, typename... Args>
    struct Function<R(T::*)(Args...) const> {
        constexpr static std::string_view signature = Function<R(std::decay_t<Args>...)>::signature;
        constexpr static std::string_view kotlin_type = Function<R(std::decay_t<Args>...)>::kotlin_type;
    };

    /**
     * Acts as a Java/Kotlin callback proxy in C++ code or a C++ function object proxy in Java/Kotlin code.
     */
    template <typename R, typename... Args>
    struct ArgType<std::function<R(Args...)>> {
        using native_type = std::function<R(Args...)>;
        using java_type = jobject;

        template <typename T>
        struct as_object {
            using type = Object;
        };
    
        constexpr static std::string_view kotlin_function_type = "Lkotlin/jvm/functions/Function";
        constexpr static std::string_view semicolon = ";";
        constexpr static std::string_view type_sig = join_v<kotlin_function_type, integer_to_digits<sizeof...(Args)>::value, semicolon>;
        constexpr static std::string_view kotlin_type = Function<R(Args...)>::kotlin_lambda_type;

    private:
        constexpr static std::string_view invoke_sig = Function<Object(typename as_object<Args>::type...)>::signature;

    public:
        static std::function<R(Args...)> native_value(JNIEnv* env, jobject value) {
            GlobalObjectRef fun = GlobalObjectRef(env, value);
            LocalClassRef cls(env, fun.ref());
            Method invoke = cls.getMethod("invoke", invoke_sig);  // lifecycle bound to object reference
            return [fun = std::move(fun), invoke = std::move(invoke)](Args... args) -> R {
                // retrieve an environment reference (which may not be the same as when the function object was created)
                JNIEnv* env = this_thread.getEnv();
                if (!env) {
                    assert(!"consistency failure");
                    if constexpr (!std::is_same_v<R, void>) {
                        return R();
                    } else {
                        return;
                    }
                }

                // Kotlin's `FunctionX` family of classes have an `invoke` method that takes and returns Object instances;
                // primitive types need boxing/unboxing
                if constexpr (!std::is_same_v<R, void>) {
                    auto objResult = LocalObjectRef(env,
                        env->CallObjectMethod(
                            fun.ref(), invoke.ref(),
                            LocalObjectRef(env,
                                ArgType<Args>::java_box(env, ArgType<Args>::java_value(env, args))
                            ).ref()...
                        )
                    );
                    if (env->ExceptionCheck()) {
                        throw JavaException(env);
                    }
                    return ArgType<R>::native_value(env, ArgType<R>::java_unbox(env, objResult.ref()));
                } else {
                    env->CallVoidMethod(
                        fun.ref(), invoke.ref(),
                        LocalObjectRef(env,
                            ArgType<Args>::java_box(env, ArgType<Args>::java_value(env, args))
                        ).ref()...
                    );
                    if (env->ExceptionCheck()) {
                        throw JavaException(env);
                    }
                }
            };
        }

        static jobject java_value(JNIEnv* env, const std::function<R(Args...)>& value) {
            throw std::runtime_error("C++ functions returning a function object to Java/Kotlin are not supported.");
        }
    };

    template <typename T>
    using java_t = typename ArgType<T>::java_type;

    template <typename L, typename T>
    L ListArgType<L, T>::native_value(JNIEnv* env, jobject list) {
        LocalClassRef listClass(env, list);

        Method sizeFunc = listClass.getMethod("size", Function<int32_t()>::signature);
        jint len = env->CallIntMethod(list, sizeFunc.ref());

        Method getFunc = listClass.getMethod("get", Function<Object(int32_t)>::signature);

        L nativeList;
        for (jint i = 0; i < len; i++) {
            LocalObjectRef listElement(env, env->CallObjectMethod(list, getFunc.ref(), i));
            nativeList.push_back(ArgType<T>::native_value(env, ArgType<T>::java_unbox(env, listElement.ref())));
        }

        return nativeList;
    }

    template <typename L, typename T>
    jobject ListArgType<L, T>::java_value(JNIEnv* env, const L& nativeList) {
        LocalClassRef arrayListClass(env, "java/util/ArrayList");
        Method initFunc = arrayListClass.getMethod("<init>", Function<void(int)>::signature);
        jobject arrayList = env->NewObject(arrayListClass.ref(), initFunc.ref(), nativeList.size());
        Method addFunc = arrayListClass.getMethod("add", Function<bool(Object)>::signature);

        for (auto&& element : nativeList) {
            LocalObjectRef arrayListElement(env, ArgType<T>::java_box(env, ArgType<T>::java_value(env, element)));
            env->CallBooleanMethod(arrayList, addFunc.ref(), arrayListElement.ref());
        }
        return arrayList;
    }

    template <typename S, typename E>
    S SetArgType<S, E>::native_value(JNIEnv* env, jobject set) {
        LocalClassRef setClass(env, set);
        Method iteratorFunc = setClass.getMethod("iterator", "()Ljava/util/Iterator;");

        LocalClassRef iteratorClass(env, "java/util/Iterator");
        Method hasNextFunc = iteratorClass.getMethod("hasNext", "()Z");
        Method nextFunc = iteratorClass.getMethod("next", "()Ljava/lang/Object;");

        LocalObjectRef setIterator(env, env->CallObjectMethod(set, iteratorFunc.ref()));

        S nativeSet;
        bool hasNext = static_cast<bool>(env->CallBooleanMethod(setIterator.ref(), hasNextFunc.ref()));
        while (hasNext) {
            LocalObjectRef setElement(env, env->CallObjectMethod(setIterator.ref(), nextFunc.ref()));
            auto&& element = ArgType<E>::java_unbox(env, setElement.ref());

            nativeSet.insert(ArgType<E>::native_value(env, element));

            hasNext = static_cast<bool>(env->CallBooleanMethod(setIterator.ref(), hasNextFunc.ref()));
        }
        return nativeSet;
    }

    template <typename S, typename E>
    jobject SetArgType<S, E>::java_value(JNIEnv* env, const S& nativeSet) {
        LocalClassRef setClass(env, ArgType<S>::concrete_class_name.data());
        Method initFunc = setClass.getMethod("<init>", Function<void()>::signature);
        Method addFunc = setClass.getMethod("add", Function<bool(Object)>::signature);
        jobject set = env->NewObject(setClass.ref(), initFunc.ref());
        if (set == nullptr) {
            throw JavaException(env);
        }

        for (auto&& item : nativeSet) {
            LocalObjectRef element(env, ArgType<E>::java_box(env, ArgType<E>::java_value(env, item)));
            env->CallBooleanMethod(set, addFunc.ref(), element.ref());
        }
        return set;
    }

    template <typename M, typename K, typename V>
    M MapArgType<M, K, V>::native_value(JNIEnv* env, jobject map) {
        LocalClassRef mapClass(env, map);
        Method entrySetFunc = mapClass.getMethod("entrySet", "()Ljava/util/Set;");

        LocalClassRef entrySetClass(env, "java/util/Set");
        Method iteratorFunc = entrySetClass.getMethod("iterator", "()Ljava/util/Iterator;");

        LocalClassRef iteratorClass(env, "java/util/Iterator");
        Method hasNextFunc = iteratorClass.getMethod("hasNext", "()Z");
        Method nextFunc = iteratorClass.getMethod("next", "()Ljava/lang/Object;");

        LocalClassRef entryClass(env, "java/util/Map$Entry");
        Method getKeyFunc = entryClass.getMethod("getKey", "()Ljava/lang/Object;");
        Method getValueFunc = entryClass.getMethod("getValue", "()Ljava/lang/Object;");

        LocalObjectRef mapEntrySet(env, env->CallObjectMethod(map, entrySetFunc.ref()));
        LocalObjectRef mapIterator(env, env->CallObjectMethod(mapEntrySet.ref(), iteratorFunc.ref()));

        M nativeMap;
        bool hasNext = static_cast<bool>(env->CallBooleanMethod(mapIterator.ref(), hasNextFunc.ref()));
        while (hasNext) {
            LocalObjectRef mapEntry(env, env->CallObjectMethod(mapIterator.ref(), nextFunc.ref()));
            LocalObjectRef mapKey(env, env->CallObjectMethod(mapEntry.ref(), getKeyFunc.ref()));
            LocalObjectRef mapValue(env, env->CallObjectMethod(mapEntry.ref(), getValueFunc.ref()));

            auto&& key = ArgType<K>::java_unbox(env, mapKey.ref());
            auto&& value = ArgType<V>::java_unbox(env, mapValue.ref());

            nativeMap[ArgType<K>::native_value(env, key)] = ArgType<V>::native_value(env, value); 

            hasNext = static_cast<bool>(env->CallBooleanMethod(mapIterator.ref(), hasNextFunc.ref()));
        }

        return nativeMap;
    }

    template <typename M, typename K, typename V>
    jobject MapArgType<M, K, V>::java_value(JNIEnv* env, const M& nativeMap) {
        LocalClassRef mapClass(env, ArgType<M>::concrete_class_name.data());
        Method initFunc = mapClass.getMethod("<init>", Function<void()>::signature);
        Method putFunc = mapClass.getMethod("put", Function<Object(Object, Object)>::signature);
        jobject map = env->NewObject(mapClass.ref(), initFunc.ref(), nativeMap.size());
        if (map == nullptr) {
            throw JavaException(env);
        }

        for (auto&& item : nativeMap) {
            LocalObjectRef key(env, ArgType<K>::java_box(env, ArgType<K>::java_value(env, item.first)));
            LocalObjectRef value(env, ArgType<V>::java_box(env, ArgType<V>::java_value(env, item.second)));
            env->CallObjectMethod(map, putFunc.ref(), key.ref(), value.ref());
        }
        return map;
    }

    template <typename...>
    struct types {
        using type = types;
    };

    /**
     * Gets a list of call argument types from a function-like type signature.
     */
    template <typename Sig>
    struct args;

    template <typename R, typename... Args>
    struct args<R(Args...)> : types<Args...> {};

    template <typename R, typename... Args>
    struct args<R(*)(Args...)> : args<R(Args...)> {};

    template <typename T, typename R, typename... Args>
    struct args<R(T::*)(Args...)> : args<R(Args...)> {};

    template <typename T, typename R, typename... Args>
    struct args<R(T::*)(Args...) const> : args<R(Args...)> {};

    template <typename Sig>
    using args_t = typename args<Sig>::type;

    template <typename F>
    struct is_free_function_pointer
        : std::integral_constant<bool, std::is_pointer_v<F> && std::is_function_v<std::remove_pointer_t<F>>>
    {};

   struct JavaOutputBuffer : std::stringbuf {
        JavaOutputBuffer(JNIEnv* env)
            : _env(env)
            , _out(LocalClassRef(env, "java/lang/System").getStaticObjectField("out", "Ljava/io/PrintStream;"))
            , _print(LocalClassRef(env, "java/io/PrintStream").getMethod("print", "(Ljava/lang/String;)V"))
        {}

        virtual int sync() {
            _env->CallVoidMethod(_out.ref(), _print.ref(), LocalObjectRef(_env, ArgType<std::string>::java_value(_env, str())).ref());
            str("");
            return 0;
        }

    private:
        JNIEnv* _env;
        LocalObjectRef _out;
        Method _print;
    };

    /**
     * Prints to the Java standard output `System.out`.
     */
    struct JavaOutput {
        JavaOutput(JNIEnv* env) : _buf(env), _str(&_buf) {}

        ~JavaOutput() {
            _buf.pubsync();
        }

        std::ostream& stream() {
            return _str;
        }

    private:
        JavaOutputBuffer _buf;
        std::ostream _str;
    };

    /**
     * Converts a native exception into a Java exception.
     */
    inline void exception_handler(JNIEnv* env, std::exception& ex) {
        // ensure that no unhandled Java exception is waiting to be thrown
        if (!env->ExceptionCheck()) {
            LocalClassRef cls(env, "java/lang/Exception");
            env->ThrowNew(cls.ref(), ex.what());
        }
    }

    /**
     * Wraps a native function pointer into a function pointer callable from Java.
     * Adapts a function with the signature R(*func)(Args...).
     * @tparam func The callable function pointer.
     * @return A type-safe function pointer to pass to Java's [RegisterNatives] function.
     */
    template <auto func, typename... Args>
    struct Adapter {
        using result_type = decltype(func(std::declval<Args>()...));

        static java_t<result_type> invoke(JNIEnv* env, jclass obj, java_t<std::decay_t<Args>>... args) {
            try {
                if constexpr (!std::is_same_v<result_type, void>) {
                    auto&& result = func(ArgType<std::decay_t<Args>>::native_value(env, args)...);
                    return ArgType<result_type>::java_value(env, std::move(result));
                } else {
                    func(ArgType<std::decay_t<Args>>::native_value(env, args)...);
                }
            } catch (JavaException& ex) {
                env->Throw(ex.innerException());
                if constexpr (!std::is_same_v<result_type, void>) {
                    return java_t<result_type>();
                }
            } catch (std::exception& ex) {
                exception_handler(env, ex);
                if constexpr (!std::is_same_v<result_type, void>) {
                    return java_t<result_type>();
                }
            }
        }
    };

    /**
     * Wraps a native member function pointer into a function pointer callable from Java.
     * Adapts a function with the signature R(T::*func)(Args...).
     * @tparam func The callable member function pointer.
     * @return A type-safe function pointer to pass to Java's [RegisterNatives] function.
     */
    template <typename T, auto func, typename... Args>
    struct MemberAdapter {
        using result_type = decltype((std::declval<T>().*func)(std::declval<Args>()...));

        static java_t<result_type> invoke(JNIEnv* env, jobject obj, java_t<std::decay_t<Args>>... args) {
            try {
                // look up field that stores native pointer
                LocalClassRef cls(env, obj);
                Field field = cls.getField("nativePointer", ArgType<T*>::type_sig.data());
                T* ptr = ArgType<T*>::native_field_value(env, obj, field);

                // invoke native function
                if (!ptr) {
                    throw std::logic_error(msg() << "Object " << ArgType<T>::class_name << " has already been disposed of.");
                }
                if constexpr (!std::is_same_v<result_type, void>) {
                    auto&& result = (ptr->*func)(ArgType<std::decay_t<Args>>::native_value(env, args)...);
                    return ArgType<result_type>::java_value(env, std::move(result));
                } else {
                    (ptr->*func)(ArgType<std::decay_t<Args>>::native_value(env, args)...);
                }
    
            } catch (JavaException& ex) {
                env->Throw(ex.innerException());
                if constexpr (!std::is_same_v<result_type, void>) {
                    return java_t<result_type>();
                }
            } catch (std::exception& ex) {
                exception_handler(env, ex);
                if constexpr (!std::is_same_v<result_type, void>) {
                    return java_t<result_type>();
                }
            }
        }
    };

    /**
     * Wraps a native function pointer or a member function pointer into a function pointer callable from Java.
     * @tparam func The callable function or member function pointer.
     * @return A type-erased function pointer to pass to Java's [RegisterNatives] function.
     */
    template <typename T, auto func, typename... Args>
    constexpr void* callable(types<Args...>) {
        auto&& f = std::conditional_t<
            std::is_member_function_pointer_v<decltype(func)>,
            MemberAdapter<T, func, Args...>,
            Adapter<func, Args...>
        >::invoke;
        return reinterpret_cast<void*>(f);
    }

    /**
     * Adapts a constructor function to be invoked from Java on object instantiation with a class method.
     */
    template <typename T, typename... Args>
    struct CreateObjectAdapter {
        static jobject invoke(JNIEnv* env, jclass cls, java_t<Args>... args) {
            try {
                // instantiate native object
                T* ptr = new T(ArgType<Args>::native_value(env, args)...);

                // instantiate Java object by skipping constructor
                LocalClassRef objClass(env, cls);
                jobject obj = env->AllocObject(objClass.ref());
                if (obj == nullptr) {
                    throw JavaException(env);
                }

                // store native pointer in Java object field
                Field field = objClass.getField("nativePointer", ArgType<T*>::type_sig.data());
                ArgType<T*>::java_field_value(env, obj, field, ptr);
        
                return obj;
            } catch (JavaException& ex) {
                env->Throw(ex.innerException());
                return nullptr;
            } catch (std::exception& ex) {
                exception_handler(env, ex);
                return nullptr;
            }
        }
    };

    /**
     * Adapts a destructor function to be invoked from Java when the object is being disposed of.
     * This function is bound to the method close() inherited from the interface AutoCloseable.
     */
    template <typename T>
    struct DestroyObjectAdapter {
        static void invoke(JNIEnv* env, jobject obj) {
            try {
                // look up field that stores native pointer
                LocalClassRef cls(env, obj);
                Field field = cls.getField("nativePointer", ArgType<T*>::type_sig.data());
                T* ptr = ArgType<T*>::native_field_value(env, obj, field);
                
                // release native object
                delete ptr;

                // prevent accidental duplicate delete
                ArgType<T*>::java_field_value(env, obj, field, nullptr);
            } catch (JavaException& ex) {
                env->Throw(ex.innerException());
            } catch (std::exception& ex) {
                exception_handler(env, ex);
            }
        }
    };

    template <typename T, typename... Args>
    constexpr void* object_initialization(types<Args...>) {
        return reinterpret_cast<void*>(CreateObjectAdapter<T, Args...>::invoke);
    }

    template <typename T>
    constexpr void* object_termination() {
        return reinterpret_cast<void*>(DestroyObjectAdapter<T>::invoke);
    }

    struct FunctionBinding {
        std::string_view name;
        std::string_view signature;
        bool is_member;
        void* function_entry_point;
        std::string_view friendly_signature;
    };

    struct FunctionBindings {
        inline static std::map< std::string_view, std::vector<FunctionBinding> > value;
    };

    /**
     * Represents a native class in Java.
     * The Java object holds an opaque pointer to the native object. The lifecycle of the object is governed by Java.
     */
    template <typename T>
    struct native_class {
        native_class() {
            auto&& bindings = FunctionBindings::value[ArgType<T>::class_name];
            bindings.push_back({
                "close",
                Function<void()>::signature,
                true,
                object_termination<T>(),
                Function<void()>::kotlin_type
            });
        }

        native_class(const native_class&) = delete;
        native_class(native_class&&) = delete;

        template <typename F>
        native_class& constructor(std::string_view name) {
            static_assert(std::is_function_v<F>, "Use a function signature such as Sample(int, std::string) to identify a constructor.");

            auto&& bindings = FunctionBindings::value[ArgType<T>::class_name];
            bindings.push_back({
                name,
                Function<F>::signature,
                false,
                object_initialization<T>(args_t<F>{}),
                Function<F>::kotlin_type
            });
            return *this;
        }

        template <auto func>
        native_class& function(const char* name) {
            using func_type = decltype(func);

            // check if signature is R(*func)(Args...)
            constexpr bool is_free = is_free_function_pointer<func_type>::value;
            // check if signature is R(T::*func)(Args...)
            constexpr bool is_member = std::is_member_function_pointer<func_type>::value;

            static_assert(is_free || is_member, "The non-type template argument is expected to be of a free function or a compatible member function pointer type.");

            auto&& bindings = FunctionBindings::value[ArgType<T>::class_name];
            bindings.push_back({
                name,
                Function<func_type>::signature,
                is_member,
                callable<T, func>(args_t<func_type>{}),
                Function<func_type>::kotlin_type
            });
            return *this;
        }
    };

    /**
     * Declares a class to serve as a data transfer type.
     * Data classes marshal data between native and Java code with copy semantics. The lifecycle of the native
     * and the Java object is not coupled.
     */
    template <typename T>
    struct data_class {
        data_class() = default;
        data_class(const data_class&) = delete;
        data_class(data_class&&) = delete;

        template <auto member>
        data_class& field(const char* name) {
            static_assert(std::is_member_object_pointer_v<decltype(member)>, "The non-type template argument is expected to be of a member variable pointer type.");
            using member_type = typename FieldType<decltype(member)>::type;

            auto&& bindings = FieldBindings::value[ArgType<T>::type_sig];
            bindings.push_back({
                name,
                ArgType<member_type>::type_sig,
                [](JNIEnv* env, jobject obj, Field& fld, const void* native_object_ptr) {
                    const T* native_object = reinterpret_cast<const T*>(native_object_ptr);
                    ArgType<member_type>::java_field_value(env, obj, fld, native_object->*member);
                },
                [](JNIEnv* env, jobject obj, Field& fld, void* native_object_ptr) {
                    T* native_object = reinterpret_cast<T*>(native_object_ptr);
                    native_object->*member = ArgType<member_type>::native_field_value(env, obj, fld);
                }
            });
            return *this;
        }
    };

    /**
     * Prints all registered Java bindings.
     */
    inline void print_registered_bindings() {
        JavaOutput output(this_thread.getEnv());
        std::ostream& os = output.stream();

        os
            << "/** Represents a class that is instantiated in native code. */\n"
            << "abstract class NativeObject : AutoCloseable {\n"
            << "    /** Holds an opaque reference to an object that exists in the native code execution context. */\n"
            << "    @Suppress(\"unused\") private val nativePointer: Long = 0\n"
            << "}\n\n"
        ;

        for (auto&& [class_name, bindings] : FunctionBindings::value) {
            std::string simple_class_name;
            std::size_t found = class_name.rfind('/');
            if (found != std::string::npos) {
                simple_class_name = class_name.substr(found + 1);
            } else {
                simple_class_name = class_name;
            }

            // class definition
            os
                << "class " << simple_class_name << " private constructor() : NativeObject() {\n"
            ;

            // instance methods
            for (auto&& binding : bindings) {
                if (binding.is_member) {
                    os << "    external fun " << binding.name << binding.friendly_signature << "\n";
                }
            }

            // companion object methods
            os << "    companion object {\n";
            for (auto&& binding : bindings) {
                if (!binding.is_member) {
                    os << "        @JvmStatic external fun " << binding.name << binding.friendly_signature << "\n";
                }
            }

            // end of class definition
            os
                << "    }\n"
                << "}\n";
        }
    }

    inline void throw_exception(JNIEnv* env, const std::string& reason) {
        if (env->ExceptionCheck()) {
            env->ExceptionClear();  // thrown by a previously failed Java call
        }
        LocalClassRef exClass(env, "java/lang/Exception");
        env->ThrowNew(exClass.ref(), reason.c_str());
    }
}

/**
 * Implements the Java [JNI_OnLoad] initialization routine.
 * @param initializer A user-defined function where bindings are registered, e.g. with [native_class].
 */
inline jint java_initialization_impl(JavaVM* vm, void (*initializer)()) {
    using namespace java;

    JNIEnv* env;
    jint rc = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (rc != JNI_OK) {
        return rc;
    }

    // register Java environment
    java::Environment::load(vm);
    java::this_thread.setEnv(env);

    try {
        // invoke user-defined function
        initializer();

        // register function bindings
        for (auto&& [class_name, bindings] : FunctionBindings::value) {
            // find the native class; JNI_OnLoad is called from the correct class loader context for this to work
            LocalClassRef cls(env, class_name.data(), std::nothrow);
            if (cls.ref() == nullptr) {
                java::throw_exception(env,
                    msg() << "Cannot find class '" << class_name << "' registered as a native class in C++ code"
                );
                return JNI_ERR;
            }

            // register native methods of the class
            std::vector<JNINativeMethod> functions;
            std::transform(bindings.begin(), bindings.end(), std::back_inserter(functions), [](auto&& binding) -> JNINativeMethod {
                return {
                    const_cast<char*>(binding.name.data()),
                    const_cast<char*>(binding.signature.data()),
                    binding.function_entry_point
                };
            });
            jint rc = env->RegisterNatives(cls.ref(), functions.data(), functions.size());
            if (rc != JNI_OK) {
                return rc;
            }
        }

        if (env->ExceptionCheck()) {
            return JNI_ERR;
        }

        // check property bindings
        for (auto&& [class_name, bindings] : FieldBindings::value) {
            // find the native class; JNI_OnLoad is called from the correct class loader context for this to work
            LocalClassRef cls(env, class_name.data(), std::nothrow);
            if (cls.ref() == nullptr) {
                java::throw_exception(env,
                    msg() << "Cannot find class '" << class_name << "' registered as a data class in C++ code"
                );
                return JNI_ERR;
            }

            // try to look up registered fields
            for (auto&& binding : bindings) {
                jfieldID ref = env->GetFieldID(cls.ref(), binding.name.data(), binding.signature.data());
                if (ref != nullptr) {
                    continue;  // everything OK, field exists
                }

                java::throw_exception(env,
                    msg() << "Cannot find field '" << binding.name << "' with type signature '" << binding.signature << "' in registered class '" << class_name << "'"
                );
                return JNI_ERR;
            }
        }
    } catch (std::exception& ex) {
        // ensure no native exception is propagated to Java
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;    
}

/**
 * Implements the Java [JNI_OnUnload] termination routine.
 */
inline void java_termination_impl(JavaVM* vm) {
    java::Environment::unload(vm);
}

/**
 * Establishes a mapping between a composite native type and a Java data class.
 * This object serves as a means to marshal data between Java and native, and is passed by value.
 */
#define DECLARE_DATA_CLASS(native_type, java_class_qualifier) \
    namespace java { \
        template <> \
        struct ArgType<native_type> : DataClassArgType<native_type> { \
            constexpr static std::string_view qualified_name = java_class_qualifier; \
            constexpr static std::string_view class_name = replace_v<qualified_name, '.', '/'>; \
        }; \
    }

/**
 * Establishes a mapping between a composite native type and a Java class.
 * This object lives primarily in the native code space, and exposed to Java as an opaque handle.
 */
#define DECLARE_NATIVE_CLASS(native_type, java_class_qualifier) \
    namespace java { \
        template <> \
        struct ArgType<native_type> : NativeClassArgType<native_type> { \
            constexpr static std::string_view qualified_name = java_class_qualifier; \
            constexpr static std::string_view class_name = replace_v<qualified_name, '.', '/'>; \
        }; \
    }

/**
 * Registers the library with Java, and binds user-defined native functions to Java instance and class methods.
 */
#define JAVA_EXTENSION_MODULE() \
    static void java_bindings_initializer(); \
    JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) { return java_initialization_impl(vm, java_bindings_initializer); } \
    JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) { java_termination_impl(vm); } \
    void java_bindings_initializer()

#define JAVA_OUTPUT ::java::JavaOutput(::java::this_thread.getEnv()).stream()
