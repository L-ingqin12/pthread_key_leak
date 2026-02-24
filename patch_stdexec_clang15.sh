#!/bin/bash
# patch_stdexec_clang15.sh
# 让 stdexec 兼容 Clang 15 + OHOS NDK

STDEXEC_DIR="${1:-/mnt/d/CLionProjects/pthread_key_leak/third_party/stdexec}"

echo "[1/3] 备份原始文件..."
cp "$STDEXEC_DIR/include/stdexec/__detail/__type_traits.hpp" \
   "$STDEXEC_DIR/include/stdexec/__detail/__type_traits.hpp.bak"
cp "$STDEXEC_DIR/include/stdexec/__detail/__meta.hpp" \
   "$STDEXEC_DIR/include/stdexec/__detail/__meta.hpp.bak"

echo "[2/3] patch __type_traits.hpp: 修复 __copy_cvref_t (Clang 15 不支持 dependent template alias)..."
# 原始写法（需要 Clang 16+）:
#   using __copy_cvref_t = __copy_cvref_fn<_From>::template __f<_To>;
# 改为两步写法（Clang 15 兼容）:
#   template<class _From, class _To>
#   using __copy_cvref_t = typename __copy_cvref_fn<_From>::template __f<_To>;
python3 - "$STDEXEC_DIR/include/stdexec/__detail/__type_traits.hpp" << 'PYEOF'
import sys, re

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

# 修复：加上 typename 关键字
old = '  using __copy_cvref_t = __copy_cvref_fn<_From>::template __f<_To>;'
new = '  using __copy_cvref_t = typename __copy_cvref_fn<_From>::template __f<_To>;'

if old in content:
    content = content.replace(old, new)
    with open(path, 'w') as f:
        f.write(content)
    print("  OK: __copy_cvref_t patched")
else:
    print("  SKIP: pattern not found (already patched?)")
    # 查看实际内容
    for i, line in enumerate(content.split('\n')):
        if 'copy_cvref_t' in line:
            print(f"  Line {i+1}: {line}")
PYEOF

echo "[3/3] patch __meta.hpp: 修复 source_location (OHOS NDK libc++15 缺少此头文件)..."
python3 - "$STDEXEC_DIR/include/stdexec/__detail/__meta.hpp" << 'PYEOF'
import sys

path = sys.argv[1]
with open(path, 'r') as f:
    content = f.read()

old = '#include <source_location> // IWYU pragma: keep for std::source_location::current'
new = '''// source_location: Clang 15 / OHOS NDK libc++ 15 compatibility
#if __has_include(<source_location>)
#  include <source_location>
#elif __has_include(<experimental/source_location>)
#  include <experimental/source_location>
namespace std { using source_location = std::experimental::source_location; }
#else
// Provide a minimal stub for compilers that lack source_location entirely
#  include <cstdint>
namespace std {
struct source_location {
    static constexpr source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint_least32_t line = __builtin_LINE(),
        uint_least32_t col  = 0) noexcept {
        source_location loc;
        loc._file = file; loc._func = func;
        loc._line = line; loc._col  = col;
        return loc;
    }
    constexpr const char*     file_name()     const noexcept { return _file; }
    constexpr const char*     function_name() const noexcept { return _func; }
    constexpr uint_least32_t  line()          const noexcept { return _line; }
    constexpr uint_least32_t  column()        const noexcept { return _col;  }
private:
    const char*     _file = "";
    const char*     _func = "";
    uint_least32_t  _line = 0;
    uint_least32_t  _col  = 0;
};
} // namespace std
#endif // source_location'''

if old in content:
    content = content.replace(old, new)
    with open(path, 'w') as f:
        f.write(content)
    print("  OK: source_location patched")
else:
    print("  SKIP: already patched")
PYEOF

echo ""
echo "Done. 验证 patch 结果："
grep -n "copy_cvref_t\|typename __copy" \
    "$STDEXEC_DIR/include/stdexec/__detail/__type_traits.hpp" | grep -v "bak"
grep -n "source_location\|has_include" \
    "$STDEXEC_DIR/include/stdexec/__detail/__meta.hpp" | head -5
