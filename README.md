# KtBind: C++ and Kotlin interoperability

KtBind is a lightweight C++17 header-only library that exposes C++ types to Kotlin and vice versa, mainly to create Kotlin bindings for existing C++ code. The objective of KtBind is to provide an easy-to-use interoperability interface that feels natural from both C++ and Kotlin. KtBind uses C++ compile-time introspection to generate Java Native Interface (JNI) stubs that can be accessed from Kotlin with regular function invocation. The stubs incur minimal or no overhead compared to hand-written JNI code.

This project has been inspired by a similar binding interface between JavaScript and C++ in [emscripten](https://emscripten.org), and between Python and C++ in [PyBind11](https://pybind11.readthedocs.io/en/stable/) and [Boost.Python](https://www.boost.org/doc/libs/1_74_0/libs/python/doc/html/index.html). Unlike [JNA](https://github.com/java-native-access/jna), which one can utilize by means of an intermediary C interface, KtBind offers a direct interface between C++ and Kotlin.

## Core features

The following core C++ features can be mapped to Kotlin:

* Functions accepting and returning custom data structures by value or by const reference
* Instance methods and static methods
* Overloaded functions
* Operators
* Instance attributes and static attributes
* Arbitrary exception types
* Callbacks and function objects
* STL data structures

Furthermore, the following core Kotlin features are exposed seamlessly to C++:

* Collection types
* Higher-order functions and lambda expressions
* Arbitrary exception types

## Getting started

KtBind is a header-only library, including the interoperability header in your C++ project allows you to create bindings to Kotlin:
```cpp
#include <ktbind/bind.hpp>
```

Consider the following C++ class as an example:
```cpp
struct Sample {
    Sample();
    Sample(const char*);
    Sample(std::string);
    Data get_data();
    void set_data(const Data& data);
};
```

All Kotlin bindings are registered in the extension module block. In order to expose the member functions of the class `Sample`, we use `native_class` and its builder functions `constructor` and `function`:
```cpp
DECLARE_NATIVE_CLASS(Sample, "com.kheiron.example.Sample")

JAVA_EXTENSION_MODULE() {
    using namespace java;
    native_class<Sample>()
        .constructor<Sample()>("create")
        .constructor<Sample(std::string)>("create")
        .function<&Sample::get_data>("get_data")
        .function<&Sample::set_data>("set_data")
    ;
    print_registered_bindings();
}
```
The type parameter of the template function `constructor` is a function signature to help choose between multiple available constructors (between constructors that take parameter types `const char*` and `std::string` in this case). The non-type template parameter of `function` is a function pointer, either a member function pointer (as shown above) or a free function pointer.

`print_registered_bindings` is a utility function that lets you print the Kotlin class definition that corresponds to the registered C++ class definitions. `print_registered_bindings` prints to Java `System.out` when you load the compiled shared library (`*.so`) with Kotlin's `System.load()`. You would normally use it in the development phase.

The bindings above map to the following class definition in Kotlin:
```kotlin
package com.kheiron.example
import com.kheiron.ktbind.NativeObject

class Sample private constructor() : NativeObject() {
    external override fun close()
    external fun get_data(): Data
    external fun set_data(data: Data)
    companion object {
        @JvmStatic external fun create(): Sample
        @JvmStatic external fun create(str: String): Sample
    }
}
```

Notice that the Kotlin functions corresponding to the C++ constructors appear in the Kotlin companion object, and the Kotlin class itself has only a private constructor. This highlights an important characteristic of `native_class`: it serves as a way to expose native objects to Kotlin. The native object lives in the C++ space, and `native_class` exposes an opaque handle to the object. (This opaque handle is stored in `NativeObject`.) Whenever a function is called, all parameters are passed by value from Kotlin to C++ by the interoperability layer, and a corresponding function invocation is made on the native object. Let's look at a specific example:
```kotlin
Sample.create().use {
    println(it.get_data())
    it.set_data(Data(true, 42, 65000, 12, 3.1416f, 2.7183, "a Kotlin string", emptyMap()))
}
```

Here, a `Sample` object is instantiated in Kotlin but a corresponding native object is immediately created in the C++ space by calling the default constructor. When a function such as `get_data()` is called, KtBind marshals all parameters, and makes an invocation to `Sample::get_data` defined in C++, and the return value is passed back to Kotlin.

C++ objects have constructors and destructors but Kotlin (JVM) has garbage collection. In order to ensure that objects are properly reclaimed when they are no longer needed, `Sample` implements the interface `AutoCloseable` (via `NativeObject`). Calling the `close` method triggers the C++ destructor. (In the example, `close()` is called automatically at the end of the `use` block.)

Notice that we have used `Data`, a type we have yet to define. Its C++ definition looks as follows:
```cpp
struct Data {
    bool b = true;
    short s = 82;
    int i = 1024;
    long l = 111000111000;
    float f = M_PI;
    double d = M_E;
    std::string str = "sample string in struct";
    std::map<std::string, std::vector<std::string>> map = {
        {"one", {"a","b","c"}},
        {"two", {"x","y","z"}},
        {"three", {}},
        {"four", {"l"}}
    };
};
```

The corresponding Kotlin definition is as shown below:
```kotlin
data class Data(
        val b: Boolean,
        val s: Short,
        val i: Int,
        val l: Long,
        val f: Float,
        val d: Double,
        val str: String,
        val map: Map<String, List<String>>
)
```

As shown in the example, KtBind supports fundamental types, object types, generic types, and composite types, up to arbitrary levels of nesting. Data classes are always passed by value, and registered in C++ with `data_class`:
```cpp
// ...
DECLARE_DATA_CLASS(Data, "com.kheiron.example.Data")

JAVA_EXTENSION_MODULE() {
    // ...
    data_class<Data>()
        .field<&Data::b>("b")
        .field<&Data::s>("s")
        .field<&Data::i>("i")
        .field<&Data::l>("l")
        .field<&Data::f>("f")
        .field<&Data::d>("d")
        .field<&Data::str>("str")
        .field<&Data::map>("map")
    ;
}
```

Both the C++ and the Kotlin definition of a data class might have fields not registered in the binding but the values of these fields will not be transferred across the language boundary, and will always take their initial values.

## Type mapping

KtBind recognizes several widely-used types and marshals them automatically between C++ and Kotlin without explicit user-defined type specification:

| C++ type | Kotlin consumed type | Kotlin produced type |
| -------- | -------------------- | -------------------- |
| `void` | n/a | `Unit` |
| `bool` | `Boolean` | `Boolean` |
| `int8_t` | `Byte` | `Byte` |
| `uint16_t` | `Character` | `Character` |
| `int16_t` | `Short` | `Short` |
| `int32_t` | `Int` | `Int` |
| `int64_t` | `Long` | `Long` |
| `int` (32-bit) | `Int` | `Int` |
| `long` (32-bit or 64-bit) | `Int` (for 32-bit) or `Long` (for 64-bit) | `Int` or `Long` |
| `long long` (64-bit) | `Long` | `Long` |
| `unsigned int` (32-bit) | `Int` | `Int` |
| `unsigned long` (32-bit or 64-bit) | `Int` (for 32-bit) or `Long` (for 64-bit) | `Int` or `Long` |
| `unsigned long long` (64-bit) | `Long` | `Long` |
| `float` | `Float` | `Float` |
| `double` | `Double` | `Double` |
| `std::string` (UTF-8) | `String` | `String` |
| `std::wstring` | `String` | `String` |
| `std::vector<T>` if `T` is an arithmetic type | `T[]` | `T[]` |
| `std::vector<T>` if `T` is not an arithmetic type | `java.util.List<T>` | `java.util.ArrayList<T>` |
| `std::list<T>` | `java.util.List<T>` | `java.util.ArrayList<T>` |
| `std::set<E>` | `java.util.Set<E>` | `java.util.TreeSet<E>` |
| `std::unordered_set<E>` | `java.util.Set<E>` | `java.util.HashSet<E>` |
| `std::map<K,V>` | `java.util.Map<K,V>` | `java.util.TreeMap<K,V>` |
| `std::unordered_map<K,V>` | `java.util.Map<K,V>` | `java.util.HashMap<K,V>` |
| `std::function<R(Args...)>` | `kotlin.jvm.functions.Function`*N* where *N* = number of `Args` | |

Java boxing an unboxing for types is performed automatically.

## Exceptions

Exceptions thrown in C++ automatically trigger a Java exception when crossing the language boundary. The interoperability layer catches all exceptions that inherit from `std::exception`, and throws a `java.lang.Exception` before passing control back to the JVM.

Exceptions originating from Java/Kotlin are automatically wrapped in a C++ type called `JavaException`, which derives from `std::exception`. The function `what()` in `JavaException` retrieves the Java exception message. C++ code can catch `JavaException` and take appropriate action, which causes the exception to be cleared in Java.

## Callbacks

Passing a callback or lambda from Kotlin to C++ is fully supported. The callback or lambda is wrapped in a `std::function<R(Args...)` that allows C++ code to trigger the lambda at any time (even from a different thread). Take the following class definition in Kotlin:
```kotlin
class LambdaSample private constructor() : NativeObject() {
    external override fun close()
    companion object {
       @JvmStatic external fun pass_callback_arguments(str: String, callback: (String, Short, Int, Long) -> String): String
    }
}
```
and the way it is used:
```kotlin
val result = LambdaSample.pass_callback_arguments("callback") { str, short, int, long -> "($str, $short, $int, $long)" }
```
The corresponding C++ function definition looks as follows:
```
std::string pass_callback_arguments(std::string str, std::function<std::string(std::string, short, int, long)> fun) {
    return fun(str, 4, 82, 112);
}
```
In the example above, `result` evaluates to `"(callback, 4, 82, 112)"`.

## Binding registration

The macro `JAVA_EXTENSION_MODULE` in KtBind expands into a pair of function definitions:
```c
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) { ... }
JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) { ... }
```
These definitions, in turn, iterate over the function and field bindings registered with `native_class` and `data_class`.

Each function binding generates a function pointer at compile time, which are passed to the JNI function `RegisterNatives`. Each of these function pointers points at a static member function of a template class, where the template parameters capture the type information extracted from the function signature. When the function is invoked through the pointer, the function makes the appropriate type conversions to cast Java types into C++ types and back. For example, the C++ function signature
```cpp
bool func(const std::string& str, const std::vector<int>& vec, double d);
```
causes the function adapter template to be instantiated with parameter types `std::string`, `std::vector<int>` and `double` and return type `bool`. When Java calls the pointer through JNI, the adapter transforms the types `std::string` and `std::vector<int>`. (`double` and `bool` need no transformation.) For each transformed type, a temporary object is created, all of which are then used in invoking the original function `func`.

Internally, field bindings utilize JNI accessor functions like `GetObjectField` and `SetObjectField` to extract and populate Java objects. Like with function bindinds, KtBind uses C++ type information to make the appropriate JNI function call. For instance, setting a field with type `double` entails a call to `GetDoubleField` (from Java to C++) or `SetDoubleField` (from C++ to Java). If the type is a composite type, such as a `std::vector<T>`, then a Java object is constructed recursively, and then set with `SetObjectField`. For example,
* Setting a field of type `std::vector<int>` first creates a `java.util.ArrayList` with JNI's `NewObject`, then sets elements with the `add` method (invoked using JNI's `CallBooleanMethod`), performing boxing for the primitive type `int` with `valueOf`, and finally uses `SetObjectField` with the newly created `java.util.ArrayList` instance.
* Setting an `std::vector<std::string>` field involves creating a `java.util.ArrayList` with JNI's `NewObject`, and a call to JNI's `NewStringUTF` for each string element. The strings are then added to the `java.util.ArrayList` instance with `add`, and finally to the field with `SetObjectField`.
