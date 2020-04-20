======================================
FogLAMP "Google IoT Core" North Plugin
======================================

This is a simple FogLAMP north plugin for sending data to the Google
Cloud Platform IoT Core.

The plugin expects to be talking to a device in GCP and requires this
to be setup in a registry in your IoT project. FogLAMP will map to a
single device within Google IoT Core, the various assets will be sent
to the topic for the FogLAMP itself.

The plugin requires a certificate and private key to communicate with
the IoT Core Gateway and also the Google root certificate. There
certificates shoud be stored in the FogLAMP certificate store.

Configuration Items
-------------------

The following configuration items are required to setup the plugin.

project_id
  The project ID of your GCP IoT Core project

region
  The GCP region that hosts your project, e.g. europe-west1

registry_id
  The registry ID in your GCP IoT Core project

device_id
  The device_id of the gateway device to which you wish to attach your devices.

key
  The name of the key associated with the gateway device, e.g. if you
  created a key file MyGatway.pem then this is simply the string MyGateway

algorithm
  The algorithm used to create your key, usually RS256

source
  The source of the data to send, usually set to readings.

Build
-----


To build FogLAMP "GCP" C++ north plugin:

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..

- By default the FogLAMP develop package header files and libraries
  are expected to be located in /usr/include/foglamp and /usr/lib/foglamp
- If **FOGLAMP_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FOGLAMP_ROOT directory.
  Please note that you must first run 'make' in the FOGLAMP_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FOGLAMP_SRC** sets the path of a FogLAMP source tree
- **FOGLAMP_INCLUDE** sets the path to FogLAMP header files
- **FOGLAMP_LIB sets** the path to FogLAMP libraries
- **FOGLAMP_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FOGLAMP_INCLUDE** option should point to a location where all the FogLAMP 
   header files have been installed in a single directory.
 - The **FOGLAMP_LIB** option should point to a location where all the FogLAMP
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FOGLAMP_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FOGLAMP_ROOT set

  $ export FOGLAMP_ROOT=/some_foglamp_setup

  $ cmake ..

- set FOGLAMP_SRC

  $ cmake -DFOGLAMP_SRC=/home/source/develop/FogLAMP  ..

- set FOGLAMP_INCLUDE

  $ cmake -DFOGLAMP_INCLUDE=/dev-package/include ..
- set FOGLAMP_LIB

  $ cmake -DFOGLAMP_LIB=/home/dev/package/lib ..
- set FOGLAMP_INSTALL

  $ cmake -DFOGLAMP_INSTALL=/home/source/develop/FogLAMP ..

  $ cmake -DFOGLAMP_INSTALL=/usr/local/foglamp ..

