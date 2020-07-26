#!/bin/bash
rootBuildPath=$1
export objectFileSet
export sourceFileExtension
export libraryFileSet

echo "=====Build C/C++ using gcc compiler====="

if [ -d output ]; then
	echo "output directory already exists"
else
	$(mkdir output)
fi


# Delete all of old output data
$(rm -rf output/*)

if [ -d temp ]; then
	echo "temp directory already exists"
	
	$(rm -rf temp/)
	$(mkdir temp)	
else
	$(mkdir temp)
fi

# Compile the files
sourceFileSet=$(ls ${rootBuildPath}/sources/*)

for sourceFile in ${sourceFileSet[@]}; do
	objectFileName=$(basename ${sourceFile})
	objectNameWithoutExtension="${objectFileName%.*}"
	sourceFileExtension="${objectFileName##*.}"

	if [ ${sourceFileExtension} == "cpp" ]; then
		if [ -d ${rootBuildPath}/headers ]; then
			g++ -c ${sourceFile} -I ${rootBuildPath}/headers/
		else	
			g++ -c ${sourceFile}
		fi
	else
		if [ -d ${rootBuildPath}/headers ]; then
			gcc -c ${sourceFile} -I ${rootBuildPath}/headers/
		else	
			gcc -c ${sourceFile}
		fi
	fi
	mv ${objectNameWithoutExtension}.o temp/

	objectFileSet="${objectFileSet} temp/${objectNameWithoutExtension}.o"
done

libSet=$(ls ${rootBuildPath}/libs/*)
for library in ${libSet[@]}; do
	libraryName=$(basename ${library})
	libraryNameWithoutExtension="${libraryName%.*}"

	libName=${libraryNameWithoutExtension:3}
	libraryFileSet="${libraryFileSet} -l${libName}"
done

if [ ${sourceFileExtension} == 'cpp' ]; then
	g++ $libraryFileSet $objectFileSet -o output/main
else
	gcc $libraryFileSet $objectFileSet -o output/main
fi

$(rm -rf temp/)