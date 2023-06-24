compile: BibakBOXServer.c BibakBOXClient.c
	gcc -Wall -pthread BibakBOXServer.c -o BibakBOXServer
	gcc -Wall -pthread BibakBOXClient.c -o BibakBOXClient


clean:
	find . -type f ! -name 'BibakBOXServer.c' ! -name 'BibakBOXClient.c' ! -name 'Makefile' ! -name 'Report.pdf' ! -name 'utils.h' ! -name 'final_project.pdf' -delete
