# trie
Auto-completion & Graph Visualization Tool

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://https://github.com/Melkor-1/trie/edit/main/LICENSE)

trie is a program that combines auto-completion and trie-based graph visualization. It enables users to suggest auto-completions for a given prefix and generates a visual representation of the underlying trie structure.

## Table of Contents

- [Trie](#trie)
- [Features](#features)
- [Building](#building)
- [Installing](#installing)
- [Usage](#usage)
- [Examples](#examples)
- [License](#license)
- [Inspiration](#inspiration)

### Trie

A [trie](https://en.wikipedia.org/wiki/Trie), or a prefix tree, is a tree-like data structure used to store a dynamic set or associative array where the keys are usually strings. 

![Trie](https://upload.wikimedia.org/wikipedia/commons/thumb/b/be/Trie_example.svg/375px-Trie_example.svg.png)

## Building 

To build the project, clone the repository and run the following commands:

```bash
git clone --depth 1 https://github.com/Melkor-1/Prefix-Tree
cd Prefix-Tree
make
```

## Installing 
The executable can be installed to `/usr/local/bin` directory by running:
```bash
# Note that this require root priveleges
make install
```

To uninstall, run:
```bash
make install
```

## Usage

The program is designed to be used from the command line:

```
./auto-complete [OPTIONS] [filename]
```

### Options:  

* -k, --keep: Keep the transient .DOT file.  
* -h, --help: Display the help message and exit.  
* -s, --svg: Generate an .SVG file for graph visualization.  
* -c, --complete PREFIX: Suggests autocompletions for a given prefix.  
* -p, --prefix PREFIX: Specify a prefix for the .DOT file. (used and required for graph generation)  


Examples:
```bash
# Keep the transient .DOT file and generate an SVG file from input.txt
./auto-complete -k -s input.txt

# Specify a prefix for the .DOT file and generate an SVG file from input.txt
./auto-complete -p prefix -s input.txt

# Suggest autocompletions for a given 'prefix' (read for stdin)
./auto-complete -c prefix 

# Display the help message
./auto-complete -h
```

## Acknowledgements
The core concepts and code structure of this library were adapted from the youtube video: [This Data Structure could be used for Autocomplete.](https://www.youtube.com/watch?v=2fosrL7I7oc)

