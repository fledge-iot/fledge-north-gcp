======================================
Fledge "Google IoT Core" North Plugin
======================================

This is a simple Fledge north plugin for sending data to the Google
Cloud Platform IoT Core.

The plugin expects to be talking to a device in GCP and requires this
to be setup in a registry in your IoT project. Fledge will map to a
single device within Google IoT Core, the various assets will be sent
to the topic for the Fledge itself.

The plugin requires a certificate and private key to communicate with
the IoT Core Gateway and also the Google root certificate. There
certificates should be stored in the Fledge certificate store.

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
  created a key file MyGateway.pem then this is simply the string MyGateway

algorithm
  The algorithm used to create your key, usually RS256

source
  The source of the data to send, usually set to readings.

Build
-----


To build Fledge "GCP" C++ north plugin:

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..

- By default the Fledge develop package header files and libraries
  are expected to be located in /usr/include/fledge and /usr/lib/fledge
- If **FLEDGE_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FLEDGE_ROOT directory.
  Please note that you must first run 'make' in the FLEDGE_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FLEDGE_SRC** sets the path of a Fledge source tree
- **FLEDGE_INCLUDE** sets the path to Fledge header files
- **FLEDGE_LIB sets** the path to Fledge libraries
- **FLEDGE_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FLEDGE_INCLUDE** option should point to a location where all the Fledge 
   header files have been installed in a single directory.
 - The **FLEDGE_LIB** option should point to a location where all the Fledge
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FLEDGE_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FLEDGE_ROOT set

  $ export FLEDGE_ROOT=/some_fledge_setup

  $ cmake ..

- set FLEDGE_SRC

  $ cmake -DFLEDGE_SRC=/home/source/develop/Fledge  ..

- set FLEDGE_INCLUDE

  $ cmake -DFLEDGE_INCLUDE=/dev-package/include ..
- set FLEDGE_LIB

  $ cmake -DFLEDGE_LIB=/home/dev/package/lib ..
- set FLEDGE_INSTALL

  $ cmake -DFLEDGE_INSTALL=/home/source/develop/Fledge ..

  $ cmake -DFLEDGE_INSTALL=/usr/local/fledge ..

