# struetype
Fork of the stb_truetype.h font header from https://github.com/nothings/stb

At this point, this repository is just an experiment to see what might be
changed to address the 2020/2022 CVE reports - upstream indicated in the
associated issues that input files are trusted by the code, and there are
currently no plans to change that.  Per the upstream header itself:

// =======================================================================
//
//    NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES
//
// This library does no range checking of the offsets found in the file,
// meaning an attacker can use it to read arbitrary memory.
//
// =======================================================================

The CVE reports in question are:

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

We'll start by setting up the test cases in the reports and making sure we can
reproduce the failures.  The code and files are from https://github.com/Vincebye
Github issues associated with these CVEs.

