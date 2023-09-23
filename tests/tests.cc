#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <unistd.h>


void print(const char *str)
{
    write(STDOUT_FILENO, str, strlen(str));
}



struct A {
    int v = 0x1234;
};

struct B {
    int nn = 0;
};

struct C : public A, public B {};

struct D : public C, public A {};

class E : private A {};

struct VA1 : virtual public A {
    A *get_aptr() { return this; }
};

struct VA2 : virtual public A {};

struct VI : public VA1, public VA2 {};

void testPtrCatch1()
{
    print("testPtrCatch1... ");
    struct A a;
    try {
        throw &a;
    }
    catch (A *e) {
        if (e != &a) abort();
        if (e->v != 0x1234) abort();
    }
    print("PASS\n");
}

void testPtrCatch2()
{
    print("testPtrCatch2... ");
    struct C c;
    try {
        throw &c;
    }
    catch (B *e) {
        if (e != (B *)&c) abort();
    }
    print("PASS\n");
}

void testVICatch()
{
    print("testVICatch... ");
    try {
        throw VI();
    }
    catch (A *a) {
        print("*** FAIL ***\n");
        return;
    }
    catch (A &a) {
    }
    print("PASS\n");
}

void testVIPtrCatch()
{
    print("testVIPtrCatch... ");
    VI vi;
    try {
        throw &vi;
    }
    catch (A &a) {
        print("*** FAIL ***\n");
        return;
    }
    catch (A *a) {
        if (a != (A *)&vi) {
            print("*** FAIL ***\n");
            return;
        }
    }
    print("PASS\n");
}

void testAmbigBaseCatch()
{
    print("testAmbigBaseCatch... ");
    try {
        throw D();
    }
    catch (A &a) { // ambiguous base
        print("*** FAIL ***\n");
        return;
    }
    catch (D &a) {
    }
    print("PASS\n");
}

void testPrivateBaseCatch()
{
    print("testPrivateBaseCatch... ");
    try {
        throw E();
    }
    catch (A &a) {
        print("*** FAIL ***\n");
        return;
    }
    catch (E &) {
    }
    print("PASS\n");
}

void testNullptrThrowCatch()
{
    print("testNullptrThrowCatch... ");
    try {
        throw nullptr;
    }
    catch (A &a) {
        print("*** FAIL ***\n");
        return;
    }
    catch (A *) {
        // (pass)
    }
    catch (...) {
        print("*** FAIL ***\n");
        return;
    }
    print("PASS\n");
}

// Tests for throwing/catching via pointer-to-pointer-to-incomplete type.
// (Note that a pointer-to-incomplete-class is also considered incomplete so cannot be thrown
// or caught; one more level of indirection is necessary!).

// These tests probably don't work as intended because the type is completed in the same
// compilation unit and the compiler likely uses the same type_info for the incomplete and
// complete type. (They still must pass of course!).

struct Inc;
Inc *incObj;
static void throwInc();

void testIncompleteTypePtrCatch()
{
    print("testIncompleteTypePtrCatch... ");
    try {
        throwInc();
    }
    catch (Inc **inc) {
        // (pass)
    }
    catch (...) {
        print("*** FAIL ***\n");
        return;
    }
    print("PASS\n");
}

static void throwInc()
{
    throw &incObj; // throw ptr-ptr-incomplete
}

static void throwIncComplete();

void testIncompleteTypePtrCatch2()
{
    print("testIncompleteTypePtrCatch2... ");
    try {
        throwIncComplete();
    }
    catch (Inc **inc) {
        // (pass)
    }
    catch (...) {
        print("*** FAIL ***\n");
        return;
    }
    print("PASS\n");
}

struct Inc {}; // type is now complete

static void throwIncComplete()
{
    throw &incObj; // throw ptr-ptr-complete
}

// TODO:
// - test general unwinding (cleanup)
// - rethrowing

int sVal = 0;

struct sValBumper {
    sValBumper() { sVal++; }
};

static void funcWithStatic()
{
    // should be initialised first time func is called (only!):
    static sValBumper bumper;
}

void testStaticInitGuard()
{
    print("testStaticInitGuard... ");
    if (sVal != 0) {
        print("*** FAIL *** (sVal != 0)\n");
        return;
    }

    // Call once; sVal should be bumped to 1
    funcWithStatic();
    if (sVal != 1) {
        print("*** FAIL *** (sVal != 1 after 1 call)\n");
        return;
    }

    // Call again; sVal should still be 1
    funcWithStatic();
    if (sVal != 1) {
        print("*** FAIL *** (sVal != 1 after 2 calls)\n");
        return;
    }
    print("PASS\n");
}

// "test" that destructors are called at exit:

struct destructAtExit {
    ~destructAtExit() {
        print("Exit-time destructor called.\n");
    }
};

destructAtExit destructMe;

// test that constructors are called at startup:

struct constructAtStart {
    static int initialised;
    constructAtStart() {
        initialised = 1;
    }
};
int constructAtStart::initialised = 0;
constructAtStart startMeUp;

void testStaticStorageConstructors()
{
    print("testStaticStorageConstructors... ");
    if (constructAtStart::initialised) {
        print("PASS\n");
    }
    else {
        print("*** FAIL ***\n");
    }
}

// extern "C" void bmcxxabi_run_init();
extern "C" void bmcxxabi_run_destructors();

int main(int argc, char **argv)
{
    // In a hosted environment, the host sytem should arrange for static-storage constructors to
    // run, so we don't need this call:
    //     bmcxxabi_run_init();

    print("BMCXXABI tests\n");

    // Exception tests
    try {
        testPtrCatch1();
        testPtrCatch2();
        testVICatch();
        testVIPtrCatch();
        testAmbigBaseCatch();
        testPrivateBaseCatch();
        testNullptrThrowCatch();
        testIncompleteTypePtrCatch();
        testIncompleteTypePtrCatch2();
    }
    catch (...) {
        puts("\n\n!!! Unexpected exception leak from test !!!\n\n");
    }

    // Other tests
    testStaticStorageConstructors();
    testStaticInitGuard();

    bmcxxabi_run_destructors();

    return EXIT_SUCCESS;
}
