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

typedef enum {
	LA_FLAVOR_TYPE_UNKNOWN,
	LA_FLAVOR_TYPE_WCSCOMLIB,
	LA_FLAVOR_TYPE_WCSSERVER,
	LA_FLAVOR_TYPE_WCSANDROIDLOGCAT,
} laFlavorType;

typedef struct wclLinesRepo wclLinesRepo;

LA_API_VISIBILITY const char** la_list_files(laFlavorType flavor, const char* folderPath, int* numFiles);

LA_API_VISIBILITY wclLinesRepo* la_init_repo_file(laFlavorType flavor, const char* filePath);
LA_API_VISIBILITY wclLinesRepo* la_init_repo_folder(laFlavorType flavor, const char* folderPath);
LA_API_VISIBILITY wclLinesRepo* la_init_repo_folder_filter(laFlavorType flavor, const char* folderPath, const char* fileNameFilterRegex);

LA_API_VISIBILITY int la_repo_num_files(wclLinesRepo* repo);
LA_API_VISIBILITY int la_repo_num_lines(wclLinesRepo* repo);
LA_API_VISIBILITY laFlavorType la_repo_flavor(wclLinesRepo* repo);

/*LA_API_VISIBILITY const char* la_lines_repo_line_content(wclLinesRepo* repo, int lineIndex, int process);

LA_API_VISIBILITY const char* la_lines_repo_available_commands(wclLinesRepo* repo);

LA_API_VISIBILITY const char* la_lines_repo_execute_command(wclLinesRepo* repo, const char* appName, const char* commandName);
LA_API_VISIBILITY const char* la_lines_repo_execute_command_params(wclLinesRepo* repo, const char* appName, const char* commandName, const char* params);*/

LA_API_VISIBILITY void la_repo_destroy(wclLinesRepo** repo);

#ifdef  __cplusplus
}
#endif

#endif
