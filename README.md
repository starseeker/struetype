# struetype
Fork of the stb_truetype.h font header from https://github.com/nothings/stb

Testing changes to address the 2020/2022 CVE reports, which are considered
out of scope by upstream. The specific CVE reports in question are:

CVE-2022-25516
CVE-2022-25515
CVE-2022-25514
CVE-2020-6623
CVE-2020-6622
CVE-2020-6621
CVE-2020-6620
CVE-2020-6619
CVE-2020-6618
CVE-2020-6617

The test cases in the reports are set up to be runnable, making sure we can
reproduce the failures.  The code and files are from https://github.com/Vincebye
Github issues associated with these CVEs.

