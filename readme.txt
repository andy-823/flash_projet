This is group project (this code is a result of group work, original repo is private).
co-authors:
https://github.com/KVV2505
https://github.com/HtWwiY
https://github.com/MathPerv

Its idea is to write files of minimal size to cover damaged blocks on flash.
Program creates something like tree and writes files of start file size.
Folder tree was created because flash starts lagging when there are many files in directory.
After no empty space left it compares written files to what had to be written.
If written file is incorrect, program replaces it with smaller files, and so on.
