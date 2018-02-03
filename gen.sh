build_flag=serial
gui_flag=
debug_flag=

#!/bin/bash
while getopts "pndch" OPTION
do
	case $OPTION in
		p)
			build_flag=parallel
			;;
		n)
			gui_flag='NO_GUI=true'
			;;
		d)
			debug_flag='DEBUG=true'
			;;
		c)
			echo "Cleaning build..."
      echo
      make clean
			;;
		\?|h)
			echo Builds and installs synaptogenesis
			echo "  usage: gen.sh [-p] [-n] [-d] [-c]"
			echo "    -p builds parallel version (requires CUDA)"
			echo "    -n builds without GUI (normally requires GTK)"
			echo "    -d builds without debug flags"
			echo "    -c cleans the build first"
			exit
			;;
	esac
done

echo ===========================
echo Building synaptogenesis ...
if [ "$build_flag" == serial ]; then
	echo "  ... serial"
else
	echo "  ... parallel"
fi

if [ "$gui_flag" == '' ]; then
	echo "  ... with GUI"
else
	echo "  ... without GUI"
fi

if [ "$debug_flag" == '' ]; then
	echo "  ... without debugging"
else
	echo "  ... with debugging"
fi
echo ===========================

echo
echo

make $build_flag $gui_flag $debug_flag

echo
echo

echo ===========================
echo Installing \(requires root\) ...
echo ===========================
sudo make install
