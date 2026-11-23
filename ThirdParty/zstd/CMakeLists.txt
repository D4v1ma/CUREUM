# Minimal, decompression-only build of Zstandard (BSD licence, see LICENSE).
# Only the decoder is needed: CUREUM reads Serum 2 presets, which store a
# zstd-compressed CBOR payload.
add_library(cureum_zstd STATIC
    lib/common/debug.c
    lib/common/entropy_common.c
    lib/common/error_private.c
    lib/common/fse_decompress.c
    lib/common/pool.c
    lib/common/threading.c
    lib/common/xxhash.c
    lib/common/zstd_common.c
    lib/decompress/huf_decompress.c
    lib/decompress/zstd_ddict.c
    lib/decompress/zstd_decompress.c
    lib/decompress/zstd_decompress_block.c)

target_include_directories(cureum_zstd PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lib")
target_compile_definitions(cureum_zstd PRIVATE ZSTD_DISABLE_ASM=1 ZSTD_LEGACY_SUPPORT=0 ZSTD_MULTITHREAD=0 XXH_NAMESPACE=CUREUM_ZSTD_)
set_target_properties(cureum_zstd PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")
