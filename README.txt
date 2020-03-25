//
// Documentation for these modules is at http://veins.car2x.org/
//

/*
* To compile, you will need to download the C++ Rest SDK - https://github.com/microsoft/cpprestsdk. When you compile the file, you will need to provide addition command line arguments like: -lboost_system -lcrypto -lssl -lcpprest. To do that, you will need to import the libraries in OMNET++:
1.  In OMNET++, click on Project-> Properties-> Paths and Symbols- add the external library in the following: 
    1.1 includes
    1.2 Libraries
    1.3 Library Paths
2.  Search for Makemake-> select the veins-src-> click on Options-> under Link tab -> add the Additional Libraries - boost_system, crypto, ssl, cpprest.
*/
