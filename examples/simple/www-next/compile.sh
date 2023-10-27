#!/bin/bash


do_check()
{
	RES=$1
	if [ ${RES} -ne 0 ]; then
		exit ${RES}
	fi
}

if [ -d ./out ]; then
	rm -rf ./out
fi

echo "Build"

npm run build
do_check $?
npm run export
do_check $?

