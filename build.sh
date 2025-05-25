#!/bin/bash

mkdir -p ./build
gcc -o ./build/msh msh.c

if [ $? -eq 0 ]; then
	echo "Compilation succesful!"
else
	echo "Compilation failed."
fi
