##npm install -g inliner

##Compress all ui files to one file
inliner ./ui/home.html > ./ui/ui.html

##Compress the result further
gzip ./ui/ui.html -c -9 > ./fs/ui.html

##Generate fsdata.c
./makefsdata