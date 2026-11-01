# C24 native selector — verification FAIL, not executed

The immutable C24 image3ee85b8492174d4c66bd513e31748e9a41388882df85808aace71f8f6fb3b592
built successfully and archived both bridge objects, but actual curl_addrinfo.o
still used legacy getaddrinfo/freeaddrinfo and final ELF had neither bridge
function. Later curl COMPILE_DEFINITIONS had overwritten the selector. Root
never ran this image to replay the known DNS failure. The corrected final
property and actual-code build postcondition are in the production source; C25
independently verifies real native selection and actual DNS/TLS behavior.
The old generated build flags were overwritten by C25 reconfiguration; they
are not regenerated or falsely archived as raw C24 evidence here.
