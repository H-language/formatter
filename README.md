# formatter

this extremely lightweight tool is designed to quickly and efficiently format text/code to align with H's code-design paradigms:
- only tabs
- all braces are column or row aligned
- inner spacing in parentheses and brackets
- no spacing between keywords and open-parentheses
- empty lines can only be 1 line
- preprocessors are indented
- backslashes in multi-lines are at the end of lines

### [download the latest here](https://github.com/H-language/formatter/releases/latest)

-------

to use:
```
formatter [text-to-format] [optional-out]
```
---
for example, to directly format H code:
```
formatter game.h
```
or if you don't want to replace the original:
```
formatter game.h game_formatted.h
```
---
to check if a file is already formatted:
```
formatter check [source-to-check]
```
this will exit with success if the file is properly formatted, or failure if it needs formatting
---
to show your current version, use:
```
formatter version
```
