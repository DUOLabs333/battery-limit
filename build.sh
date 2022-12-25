rm -rf battery
clang++ main.cpp smc.cpp -mmacosx-version-min=10.4  -Wall -std=gnu++17 -framework IOKit -o battery
for var in "$@"
do
    if [ "$var" = "--release" ]; then
    	sudo chown root battery
    	sudo chmod u+s battery
    	break
    fi
done
