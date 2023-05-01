#pragma once
#include <furry.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extract filename without extension from path.
 * 
 * @param path path string
 * @param filename output filename string. Must be initialized before.
 */
void path_extract_filename_no_ext(const char* path, FurryString* filename);

/**
 * @brief Extract filename string from path.
 * 
 * @param path path string
 * @param filename output filename string. Must be initialized before.
 * @param trim_ext true - get filename without extension
 */
void path_extract_filename(FurryString* path, FurryString* filename, bool trim_ext);

/**
 * @brief Extract file extension from path.
 * 
 * @param path path string
 * @param ext output extension string
 * @param ext_len_max maximum extension string length
 */
void path_extract_extension(FurryString* path, char* ext, size_t ext_len_max);

/**
 * @brief Extract last path component
 * 
 * @param path path string
 * @param filename output string. Must be initialized before.
 */
void path_extract_basename(const char* path, FurryString* basename);

/**
 * @brief Extract path, except for last component
 * 
 * @param path path string
 * @param filename output string. Must be initialized before.
 */
void path_extract_dirname(const char* path, FurryString* dirname);

/**
 * @brief Appends new component to path, adding path delimiter
 * 
 * @param path path string
 * @param suffix path part to apply
 */
void path_append(FurryString* path, const char* suffix);

/**
 * @brief Appends new component to path, adding path delimiter
 * 
 * @param path first path part
 * @param suffix second path part
 * @param out_path output string to combine parts into. Must be initialized
 */
void path_concat(const char* path, const char* suffix, FurryString* out_path);

/**
 * @brief Check that path contains only ascii characters
 * 
 * @param path 
 * @return true 
 * @return false 
 */
bool path_contains_only_ascii(const char* path);

#ifdef __cplusplus
}
#endif
