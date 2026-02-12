cd ../extraction/app/build_x86_64/src/          # Move to the required folder.
                                                # Iterate through all cpp files and run using gcov.
for file in $(find . -type f -name "*.gcda")
do
	gcov $file

done

pwd
ls

lcov --capture --directory . --output-file verbose.info
lcov --remove verbose.info -o coverage.info     "/usr/*"     "*/extraction/egalito/*"
genhtml coverage.info --output-directory out
