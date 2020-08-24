plugins {
    id("java")
    id("application")
    id("org.jetbrains.kotlin.jvm") version "1.4.0"
}

group = "com.kheiron.ktbind"
version = "1.0.0"

apply(plugin = "java")
apply(plugin = "application")
apply(plugin = "org.jetbrains.kotlin.jvm")

repositories {
    mavenCentral()
}

dependencies {
    // Kotlin standard library
    implementation("org.jetbrains.kotlin:kotlin-stdlib-jdk8")

    // Logging
    implementation("org.apache.logging.log4j:log4j-api:2.13.0")
    implementation("org.apache.logging.log4j:log4j-core:2.13.0")
    runtimeOnly("org.apache.logging.log4j:log4j-slf4j18-impl:2.13.0")

    // Testing
    testImplementation("org.junit.jupiter:junit-jupiter-api:5.6.0")
    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine:5.6.0")
    testImplementation("io.mockk:mockk:1.10.0+")
}

val compileKotlin: org.jetbrains.kotlin.gradle.tasks.KotlinCompile by tasks
val compileTestKotlin: org.jetbrains.kotlin.gradle.tasks.KotlinCompile by tasks

compileKotlin.kotlinOptions {
    jvmTarget = "11"
}
compileTestKotlin.kotlinOptions {
    jvmTarget = "11"
}
java {
    sourceCompatibility = JavaVersion.VERSION_11
    targetCompatibility = JavaVersion.VERSION_11
}

tasks.named<Test>("test") {
    useJUnitPlatform {
        systemProperty("junit.jupiter.testinstance.lifecycle.default", "per_class")
    }
    testLogging {
        events("passed", "skipped", "failed")
    }
}
