#pragma once

#include <stdio.h>
#include <stdlib.h>

static inline bool url_scanf(const char *url, char *proto, char *host, int *port, char *page){
	if (sscanf(url, "%99[^:]://%99[^:]:%i/%199[^\n]", proto, host, port, page) == 4) return true; 
	else if (sscanf(url, "%99[^:]://%99[^/]/%199[^\n]", proto, host, page) == 3) return true; 
	else if (sscanf(url, "%99[^:]://%99[^:]:%i[^\n]", proto, host, port) == 3) return true; 
	else if (sscanf(url, "%99[^:]://%99[^\n]", proto, host) == 2) return true; 
	return false; 	
}

