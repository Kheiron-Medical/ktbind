package com.kheiron.ktbind

import org.junit.jupiter.api.Assertions.*
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.PrintStream
import kotlin.concurrent.thread

/**
 * Represents a class that is instantiated in native code.
 */
abstract class NativeObject : AutoCloseable {
    /**
     * Holds a reference to an object that exists in the native code execution context.
     */
    @Suppress("unused")
    private val nativePointer: Long = 0
}

data class Data(
        val b: Boolean = false,
        val s: Short = 0,
        val i: Int = 0,
        val l: Long = 0,
        val f: Float = 0.0f,
        val d: Double = 0.0,
        val str: String = "",
        val short_arr: ShortArray = ShortArray(0),
        val int_arr: IntArray = IntArray(0),
        val long_arr: LongArray = LongArray(0),
        val map: Map<String, List<String>> = emptyMap()

        fun double(x: Int): Int {
            return 7
        }
)

class Sample private constructor() : NativeObject() {
    external override fun close()
    external fun get_data(): Data
    external fun set_data(data: Data)
    external fun duplicate(): Sample
    companion object {
        @JvmStatic external fun create(): Sample
        @JvmStatic external fun create(str: String): Sample
        @JvmStatic external fun returns_void()
        @JvmStatic external fun returns_bool(): Boolean
        @JvmStatic external fun returns_short(): Short
        @JvmStatic external fun returns_int(): Int
        @JvmStatic external fun returns_long(): Long
        @JvmStatic external fun returns_int16(): Short
        @JvmStatic external fun returns_int32(): Int
        @JvmStatic external fun returns_int64(): Long
        @JvmStatic external fun returns_float(): Float
        @JvmStatic external fun returns_double(): Double
        @JvmStatic external fun returns_string(): String
        @JvmStatic external fun pass_arguments_by_value(str: String, b: Boolean, s: Short, i: Int, l: Long, i16: Short, i32: Int, i64: Long, f: Float, d: Double): Boolean
        @JvmStatic external fun pass_arguments_by_reference(str: String, b: Boolean, s: Short, i: Int, l: Long, i16: Short, i32: Int, i64: Long, f: Float, d: Double): Boolean
        @JvmStatic external fun array_of_char(list: ByteArray): ByteArray
        @JvmStatic external fun array_of_int(list: IntArray): IntArray
        @JvmStatic external fun array_of_string(list: List<String>): List<String>
        @JvmStatic external fun list_of_int(list: List<Int>): List<Int>
        @JvmStatic external fun list_of_string(list: List<String>): List<String>
        @JvmStatic external fun unordered_set(set: Set<String>): Set<String>
        @JvmStatic external fun ordered_set(set: Set<String>): Set<String>
        @JvmStatic external fun unordered_map(map: Map<String, String>): Map<String, String>
        @JvmStatic external fun ordered_map_of_int(map: Map<Long, Long>): Map<Long, Long>
        @JvmStatic external fun ordered_map_of_string(map: Map<String, String>): Map<String, String>
        @JvmStatic external fun native_composite(map: Map<String, List<String>>): Map<String, List<String>>
        @JvmStatic external fun pass_callback(callback: () -> Unit)
        @JvmStatic external fun pass_callback_returns_string(callback: () -> String): String
        @JvmStatic external fun pass_callback_string_returns_int(str: String, callback: (String) -> Int): Int
        @JvmStatic external fun pass_callback_string_returns_string(str: String, callback: (String) -> String): String
        @JvmStatic external fun pass_callback_arguments(str: String, callback: (String, Short, Int, Long) -> String): String
        @JvmStatic external fun callback_on_native_thread(callback: () -> Unit)
        @JvmStatic external fun raise_native_exception()
        @JvmStatic external fun catch_java_exception(callback: () -> Unit)
    }
}

fun captureOutput(executable: () -> Unit): String {
    return ByteArrayOutputStream().use { stream ->
        val stdout = System.out
        try {
            System.setOut(PrintStream(stream))
            executable()
        } finally {
            System.setOut(stdout)
        }
        stream.toString()
    }.trimEnd()
}

fun assertPrints(expected: String, executable: () -> Unit) {
    assertEquals(expected, captureOutput(executable))
}

fun assertPrints(expected: Regex, executable: () -> Unit) {
    assertTrue(expected.matches(captureOutput(executable)))
}

internal class NativeBindingsTest {
    init {
        val libraryFile = File(System.getProperty("user.dir"), "../cpp/build/libktbind_java.so").absolutePath
        System.load(libraryFile)
    }

    @Test
    fun `lambda signatures`() {
        val fn: (String, Int, Long) -> String = { str, int, long -> "$str = $int + $long" }
        val c: Class<*> = fn::class.java
        println(c.name)
        for (classInterface in c.interfaces) {
            println(classInterface)
        }
        for (method in c.declaredMethods) {
            val methodName = method.name
            val signature = method.parameters.joinToString(separator = ", ") {
                "${it.name}: ${it.type}"
            }
            val returnType = method.returnType
            println("$methodName($signature): $returnType")
        }
    }

    /**
     * Tests fundamental and well-known types.
     */
    @Test
    fun `fundamental types`() {
        Sample.returns_void()
        assertEquals(true, Sample.returns_bool())
        assertEquals(32767, Sample.returns_short())
        assertEquals(2147483647, Sample.returns_int())
        assertEquals(9223372036854775807, Sample.returns_long())
        assertEquals(32767, Sample.returns_int16())
        assertEquals(2147483647, Sample.returns_int32())
        assertEquals(9223372036854775807, Sample.returns_int64())
        assertEquals(3.4028235E38f, Sample.returns_float())
        assertEquals(1.7976931348623157E308, Sample.returns_double())
        assertEquals("a sample string", Sample.returns_string())

        val ref = "(string = string, bool = 1, short = 32000, int = 128000, long = 1000000000, int16_t = 32000, int32_t = 128000, int64_t = 1000000000, float = 3.14, double = 3.14)"
        assertPrints(ref) {
            Sample.pass_arguments_by_value("string", true, 32000, 128000, 1000000000, 32000, 128000, 1000000000, 3.14f, 3.14)
        }
        assertPrints(ref) {
            Sample.pass_arguments_by_reference("string", true, 32000, 128000, 1000000000, 32000, 128000, 1000000000, 3.14f, 3.14)
        }
    }

    @Test
    fun `passing and returning collections`() {
        assertPrints("[$, a, b, c]") {
            assertArrayEquals(charArrayOf('a', 'b', 'c', 'd', 'e', 'f').map { it.toByte() }.toByteArray(), Sample.array_of_char(byteArrayOf('$'.toByte(), 'a'.toByte(), 'b'.toByte(), 'c'.toByte())))
        }
        assertPrints("[0, 0, 1, 1, 2, 3, 5, 8, 13]") {
            assertArrayEquals(intArrayOf(0, 1, 2, 3, 4, 5, 6), Sample.array_of_int(intArrayOf(0, 0, 1, 1, 2, 3, 5, 8, 13)))
        }
        assertPrints("[$, a, A, b, B, c, C]") {
            assertIterableEquals(listOf("", "A", "B", "C", "D", "E", "F"), Sample.array_of_string(listOf("$", "a", "A", "b", "B", "c", "C")))
        }
        assertPrints("[1, 1, 2, 3, 5, 8, 13]") {
            assertIterableEquals(listOf(1, 2, 3, 4, 5, 6), Sample.list_of_int(listOf(1, 1, 2, 3, 5, 8, 13)))
        }
        assertPrints("[a, A, b, B, c, C]") {
            assertIterableEquals(listOf("A", "B", "C", "D", "E", "F"), Sample.list_of_string(listOf("a", "A", "b", "B", "c", "C")))
        }
        assertPrints(Regex("""\{(a|b|c), (a|b|c), (a|b|c)}""")) {
            assertTrue(setOf("A", "B", "C") == Sample.unordered_set(setOf("a", "b", "c")))
        }
        assertPrints("{a, b, c}") {
            assertTrue(setOf("A", "B", "C") == Sample.ordered_set(setOf("a", "b", "c")))
        }
        assertPrints(Regex("""\{(a: b|b: c|c: d), (a: b|b: c|c: d), (a: b|b: c|c: d)}""")) {
            assertTrue(mapOf("1" to "A", "2" to "B", "3" to "C") == Sample.unordered_map(mapOf("a" to "b", "b" to "c", "c" to "d")))
        }
        assertPrints("{1: 2, 2: 3, 3: 4}") {
            assertTrue(mapOf(1L to 1000L, 2L to 2000L, 3L to 3000L) == Sample.ordered_map_of_int(mapOf(1L to 2L, 2L to 3L, 3L to 4L)))
        }
        assertPrints("{a: b, b: c, c: d}") {
            assertTrue(mapOf("1" to "A", "2" to "B", "3" to "C") == Sample.ordered_map_of_string(mapOf("a" to "b", "b" to "c", "c" to "d")))
        }
        assertPrints("{a: [1, 2, 3], b: [], c: [4]}") {
            val result = Sample.native_composite(mapOf("a" to listOf("1", "2", "3"), "b" to emptyList(), "c" to listOf("4")))
            assertIterableEquals(listOf("a", "b", "c"), result["A"]);
            assertIterableEquals(emptyList<String>(), result["B"]);
            assertIterableEquals(listOf("x"), result["C"])
        }
    }

    @Test
    fun `callback functions`() {
        assertPrints("void callback") {
            Sample.pass_callback { println("void callback") }
        }
        assertEquals("alma", Sample.pass_callback_returns_string { "alma" })
        assertEquals(82, Sample.pass_callback_string_returns_int("callback") { 82 })
        assertEquals("cpp(callback) -> kotlin(callback)",
                Sample.pass_callback_string_returns_string("callback") { "cpp($it) -> kotlin($it)" })
        assertEquals("(callback, 4, 82, 112)",
                Sample.pass_callback_arguments("callback") { str, short, int, long -> "($str, $short, $int, $long)" })

        // callback functions from new thread
        thread {
            assertPrints("[1, 1, 2, 3, 5, 8, 13]") {
                assertIterableEquals(listOf(1, 2, 3, 4, 5, 6), Sample.list_of_int(listOf(1, 1, 2, 3, 5, 8, 13)))
            }
            assertPrints("void callback") {
                Sample.pass_callback { println("void callback") }
            }
            assertEquals("alma", Sample.pass_callback_returns_string { "alma" })
            assertEquals(82, Sample.pass_callback_string_returns_int("callback") { 82 })
            assertEquals("cpp(callback) -> kotlin(callback)",
                    Sample.pass_callback_string_returns_string("callback") { "cpp($it) -> kotlin($it)" })
            assertEquals("(callback, 4, 82, 112)",
                    Sample.pass_callback_arguments("callback") { str, short, int, long -> "($str, $short, $int, $long)" })
        }.join()

        Sample.callback_on_native_thread { println("executed on native thread") }
    }

    @Test
    fun `native and java exceptions`() {
        assertThrows<Exception> {
            Sample.raise_native_exception()
        }
        assertThrows<Exception> {
            Sample.pass_callback { throw Exception("non-critical error") }
        }
        assertPrints("exception caught: non-critical error") {
            assertDoesNotThrow {
                Sample.catch_java_exception { throw Exception("non-critical error") }
            }
        }
    }

    @Test
    fun `object creation`() {
        assertPrints("""
            created
            get nested data
            Kotlin: Data(b=true, s=82, i=1024, l=111000111000, f=3.1415927, d=2.718281828459045, str=sample string in struct, short_arr=[1, 2, 3, 4, 5, 6, 7, 8, 9], int_arr=[10, 20, 30, 40, 50, 60, 70, 80, 90], long_arr=[100, 200, 300, 400, 500, 600, 700, 800, 900], map={four=[l], one=[a, b, c], three=[], two=[x, y, z]})
            set nested data: {b=1, s=42, i=65000, l=12, f=3.14, d=3.14, str='a Kotlin string'}
            destroyed
        """.trimIndent()) {
            Sample.create().use {
                println("Kotlin: ${it.get_data()}")
                it.set_data(Data(true, 42, 65000, 12, 3.14f, 3.14, "a Kotlin string", ShortArray(0), IntArray(0), LongArray(0), emptyMap()))
            }
        }

        assertPrints("""
            created from string
            destroyed
        """.trimIndent()) {
            Sample.create("parameter").use {}
        }

        Sample.create().use {
            val other = it.duplicate()
            it.set_data(Data())
            val data = it.get_data()
            assertEquals(false, data.b)
            assertEquals(0, data.s)
            assertEquals(0, data.i)
            assertEquals(0, data.l)
            assertEquals("", data.str)
            assertTrue(data.map.isEmpty())
            val data_other = other.get_data()
            assertEquals(true, data_other.b)
            assertEquals(82, data_other.s)
            assertEquals(1024, data_other.i)
            assertEquals(111000111000, data_other.l)
            assertEquals("sample string in struct", data_other.str)
            assertFalse(data_other.map.isEmpty())
            other.close()
        }
    }
}
