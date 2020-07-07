#ifndef LOG_ANALYZER_H
#define LOG_ANALYZER_H

#if defined _WIN32
	#if defined(LA_BUILDING_SHARED)
		#define LA_API_VISIBILITY __declspec(dllexport)
	#else
		#define LA_API_VISIBILITY __declspec(dllimport)
	#endif
#else
	#if defined(LA_BUILDING_SHARED)
		#define LA_API_VISIBILITY __attribute__ ((visibility ("default")))
	#else
		#define LA_API_VISIBILITY
	#endif
#endif

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
	LA_FLAVOR_TYPE_UNKNOWN,
	LA_FLAVOR_TYPE_WCSCOMLIB,
	LA_FLAVOR_TYPE_WCSSERVER,
	LA_FLAVOR_TYPE_WCSANDROIDLOGCAT
} laFlavorType;

typedef enum {
	LA_TRANSLATOR_TYPE_RAW,
	LA_TRANSLATOR_TYPE_TRANSLATED
} laTranslatorType;

typedef enum {
	LA_TRANSLATOR_FORMAT_LINE,
	LA_TRANSLATOR_FORMAT_JSON_FULL,
	LA_TRANSLATOR_FORMAT_JSON_SINGLE_PARAMS
} laTranslatorFormat;

typedef enum {
	LA_SEARCH_OPTION_NONE,
	LA_SEARCH_OPTION_CASE_SENSITIVE
} laSearchOptions;

typedef struct
{
	char* data;
	int size;
} laStrUTF8;

typedef struct
{
	const char* data;
	int size;
} laStrFixedUTF8;

typedef struct
{
	int8_t appendToFile;
	laStrFixedUTF8 filePath;
	laTranslatorType translationType;
	laTranslatorFormat translationFormat;
} laExportOptions;

typedef struct wclFindContext wclFindContext;

typedef struct wclLinesRepo wclLinesRepo;

/********
** Utils
********/

LA_API_VISIBILITY laStrUTF8 la_str_init();
LA_API_VISIBILITY laStrUTF8 la_str_init_copy(const char* cstr);
LA_API_VISIBILITY laStrUTF8 la_str_init_length(int size);
LA_API_VISIBILITY void la_str_destroy(laStrUTF8* str);

LA_API_VISIBILITY laStrFixedUTF8 la_str_fixed_init();
LA_API_VISIBILITY laStrFixedUTF8 la_str_fixed_init_str(laStrUTF8 str);
LA_API_VISIBILITY laStrFixedUTF8 la_str_fixed_init_cstr(const char* cstr);

LA_API_VISIBILITY int la_find_ctx_valid(const wclFindContext* ctx);
LA_API_VISIBILITY laStrUTF8 la_find_ctx_query(const wclFindContext* ctx);
LA_API_VISIBILITY int la_find_ctx_line_position(const wclFindContext* ctx, int* lineIndex);


/********
** Repo init and file search
********/

LA_API_VISIBILITY laStrUTF8* la_list_files(laFlavorType flavor, laStrFixedUTF8 folderPath, int* numFiles);

LA_API_VISIBILITY wclLinesRepo* la_init_repo_file(laFlavorType flavor, laStrFixedUTF8 filePath);
LA_API_VISIBILITY wclLinesRepo* la_init_repo_folder(laFlavorType flavor, laStrFixedUTF8 folderPath);
LA_API_VISIBILITY wclLinesRepo* la_init_repo_folder_filter(laFlavorType flavor, laStrFixedUTF8 folderPath, laStrFixedUTF8 fileNameFilterRegex);

LA_API_VISIBILITY wclLinesRepo* la_init_repo_command(wclLinesRepo* repo, laStrFixedUTF8 commandResult);

LA_API_VISIBILITY void la_repo_destroy(wclLinesRepo* repo);

/********
** Repo operations
********/

LA_API_VISIBILITY int la_repo_num_files(wclLinesRepo* repo);
LA_API_VISIBILITY int la_repo_num_lines(wclLinesRepo* repo);
LA_API_VISIBILITY laFlavorType la_repo_flavor(wclLinesRepo* repo);

LA_API_VISIBILITY wclFindContext* la_repo_search_text(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions);
LA_API_VISIBILITY wclFindContext* la_repo_search_text_regex(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions);
LA_API_VISIBILITY void la_repo_search_next(wclLinesRepo* repo, wclFindContext* ctx);
LA_API_VISIBILITY void la_repo_search_destroy(wclFindContext* ctx);

LA_API_VISIBILITY laStrUTF8 la_repo_find_all(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions);
LA_API_VISIBILITY laStrUTF8 la_repo_find_all_regex(wclLinesRepo* repo, laStrFixedUTF8 query, laSearchOptions searchOptions);

LA_API_VISIBILITY laStrUTF8 la_repo_retrieve_line_content(wclLinesRepo* repo, int lineIndex, laTranslatorType translatorType, laTranslatorFormat translatorFormat);

LA_API_VISIBILITY int la_repo_get_lineIndex(wclLinesRepo* repo, int32_t lineId, int* lineIndex);
LA_API_VISIBILITY laStrUTF8 la_repo_get_summary(wclLinesRepo* repo);
LA_API_VISIBILITY laStrUTF8 la_repo_get_available_commands(wclLinesRepo* repo);

LA_API_VISIBILITY laStrUTF8 la_repo_execute_command(wclLinesRepo* repo, laStrFixedUTF8 tag, laStrFixedUTF8 name);
LA_API_VISIBILITY laStrUTF8 la_repo_execute_command_params(wclLinesRepo* repo, laStrFixedUTF8 tag, laStrFixedUTF8 name, laStrFixedUTF8 params);

LA_API_VISIBILITY int la_repo_export_lines(wclLinesRepo* repo, const laExportOptions* options, int indexStart, int count);
LA_API_VISIBILITY int la_repo_export_command_lines(wclLinesRepo* repo, const laExportOptions* options, laStrFixedUTF8 commandResult);
LA_API_VISIBILITY int la_repo_export_command_network_packets(wclLinesRepo* repo, const laExportOptions* options, laStrFixedUTF8 commandResult);

#ifdef  __cplusplus
}
#endif

#endif
