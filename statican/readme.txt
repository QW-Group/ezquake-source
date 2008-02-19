Here you can use static code analysis tools. See results of their analysis or run a new analysis and see the list of newly discovered issues.

./flawfind/flawfinder.sh
  Creates flawfind/lists/flaws_new.html, which is a list of newly detected issues, not included in the list of "base" issues

./rats/rats.sh
  Creates rats/lists/rats_new.txt, which is a list of newly detected issues, better to say a difference file between the list of "base" RATS issues and newly generated list

flawfind/lists/flaws_base.html and rats/lists/rats_all.html are lists of all the issues found by the static code analysis.

Project manager should update rats_base.txt and flaws_base.list regularly.
