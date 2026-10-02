#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef unsigned long long VALUE;
extern void ruby_sysinit(int *, char ***);
extern int ruby_setup(void);
extern void ruby_init_stack(void *);
extern void *ruby_options(int, char **);
extern int ruby_executable_node(void *, int *);
extern int ruby_exec_node(void *);
extern VALUE rb_eval_string_protect(const char *, int *);
extern VALUE rb_errinfo(void);
extern void rb_set_errinfo(VALUE);
extern const char *rb_obj_classname(VALUE);

static volatile int g_stage = 1;
static volatile int g_res[8];
static volatile int g_errclass = 0;
static const wchar_t *g_path = 0;

static void mark(int s)
{
    g_stage = s;
    if (g_path) {
        FILE *f = _wfopen(g_path, L"w");
        if (f) { fprintf(f, "%d", s); fclose(f); }
    }
}

static int classcode(const char *n)
{
    if (!n) return 12;
    if (!strcmp(n, "NoMethodError")) return 1;
    if (!strcmp(n, "NameError")) return 2;
    if (!strcmp(n, "NotImplementedError")) return 3;
    if (!strcmp(n, "LoadError")) return 4;
    if (!strcmp(n, "RuntimeError")) return 5;
    if (!strcmp(n, "TypeError")) return 6;
    if (!strcmp(n, "ArgumentError")) return 7;
    if (!strcmp(n, "SystemStackError")) return 8;
    if (!strcmp(n, "NoMemoryError")) return 9;
    if (!strcmp(n, "ScriptError") || !strcmp(n, "SyntaxError")) return 10;
    if (!strncmp(n, "Errno::", 7) || !strcmp(n, "SystemCallError")) return 11;
    return 12;
}

static void run_test(int idx, const char *code, long expect)
{
    int state = 0;
    VALUE v;
    mark(5 + idx);
    v = rb_eval_string_protect(code, &state);
    if (state == 0 && v == (VALUE)(2 * expect + 1)) {
        g_res[idx] = 1;
        return;
    }
    g_res[idx] = 2;
    if (g_errclass == 0) {
        if (state != 0) {
            VALUE e = rb_errinfo();
            g_errclass = classcode(rb_obj_classname(e));
            rb_set_errinfo((VALUE)8);
        } else {
            g_errclass = 13;
        }
    }
}

__declspec(dllexport) int get_stage(void) { return g_stage; }
__declspec(dllexport) int get_err(void) { return g_errclass; }
__declspec(dllexport) int get_res(int i) { return (i >= 0 && i < 8) ? g_res[i] : 0; }

__declspec(dllexport) int run_probe(const wchar_t *logpath)
{
    int local = 0;
    int argc = 0;
    char **argv = 0;
    int passed = 0;
    int i;
    char a0[] = "mkxp-z";
    char a1[] = "-e ";
    char *av[3];
    void *node;
    int st = 0;
    int valid;

    g_path = logpath;
    mark(2);
    ruby_sysinit(&argc, &argv);
    mark(3);
    ruby_init_stack(&local);
    if (ruby_setup() != 0) return -4;
    mark(4);
    av[0] = a0;
    av[1] = a1;
    av[2] = 0;
    node = ruby_options(2, av);
    valid = ruby_executable_node(node, &st);
    if (valid) st = ruby_exec_node(node);
    if (st || !valid) return -7;

    run_test(0, "1+1", 2);
    run_test(1, "GC.start; 7", 7);
    run_test(2, "Marshal.load(Marshal.dump([1,2,3])).size", 3);
    run_test(3, "require 'zlib'; Zlib.crc32('a') & 255", 67);
    run_test(4, "Thread.new { 5 }.value", 5);
    run_test(5, "Time.now.year > 2000 ? 1 : 0", 1);
    run_test(6, "File.write('probe_test.txt', 'x'); File.read('probe_test.txt').size", 1);
    run_test(7, "(1..100).map { |i| i.to_s * 3 }.join.size", 576);
    mark(13);
    for (i = 0; i < 8; i++) if (g_res[i] == 1) passed++;
    return passed;
}
