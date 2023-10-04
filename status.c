/*
   Copyright 2023 Jamie Dennis

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <ctype.h>

static int status_date_and_time(char *buffer, int max) {
	time_t time_value = time(NULL);
	struct tm *calender_time = gmtime(&time_value);
	return strftime(buffer, max, "%a %d %T", calender_time);
}

static int status_memory_usage(char *buffer, int max) {
	FILE *meminfo = fopen("/proc/meminfo", "r");
	if (!meminfo) return 0;
	int ret = 0;
    
	unsigned long total_memory_kb = 0, used_memory_kb = 0;
	char line[128];
    
	while (fgets(line, sizeof(line), meminfo)) {
		char *value_string;
		unsigned long value;
        
		value_string = strchr(line, ':');
		if (!value_string) continue;
        
		for (; *value_string; ++value_string) {
			if (isdigit(*value_string)) break;
		}
        
		value = atoll(value_string);
        
		if (strstr(line, "MemTotal") == line) total_memory_kb = value;
		else if (strstr(line, "MemFree") == line) used_memory_kb += value;
		else if (strstr(line, "SReclaimable") == line) used_memory_kb += value;
		else if (strstr(line, "Buffers") == line) used_memory_kb += value;
		else if (strstr(line, "Cached") == line) used_memory_kb += value;
	}
    
	ret = snprintf(buffer, max, "%.2g/%.2gGB", 
				   (total_memory_kb - used_memory_kb) / (double)(1<<20),
				   total_memory_kb / (double)(1<<20));
    
	fclose(meminfo);
	return ret;
}

#ifdef ENABLE_MPD_STATUS
#include <mpd/connection.h>
#include <mpd/client.h>
#include <mpd/song.h>
static int status_mpd(char *buffer, int max) {
	int ret = 0;
	struct mpd_connection *mpd;
	mpd = mpd_connection_new(NULL, 0, 0);
	
	if (!mpd) return 0;
	
	if (mpd_connection_get_error(mpd) != MPD_ERROR_SUCCESS) {
		printf("Failed to connect to MPD: %s\n", mpd_connection_get_error_message(mpd));
		mpd_connection_free(mpd);
		return 0;
	}
	
	struct mpd_song *song = mpd_run_current_song(mpd);
	
	if (song) {
		const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
		const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
		
		if (!artist || !title) {
			const char *uri = mpd_song_get_uri(song);
			title = strrchr(uri, '/');
			if (title) title++;
			else title = uri;
			ret = snprintf(buffer, max, "%s", title);
		}
		else {
			ret = snprintf(buffer, max, "%s - %s", artist, title);
		}
	}
	
	mpd_song_free(song);
	mpd_connection_free(mpd);
	return ret;
}
#endif



