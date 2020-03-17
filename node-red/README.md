# Management Reference Application (NodeRed)

## Overview

## What you need before getting started

### Install Node.js & npm

For example, get it from here: https://www.npmjs.com/get-npm

Update to latest npm:
````
$ npm install npm@latest -g
````
Note: on a Mac, if you run into '... missing write access ...' to the npm folders, you could do this:
````
$ cd /usr/local/lib
$ sudo chmod -R ugo+w ./node_modules
<type in your password>

$ cd /usr/local/share
$ sudo chmod -R ugo+w ./man

$ cd /usr/local
$ sudo chmod ugo+w ./bin

<now install npm again>

````

### Install Node-RED

Check: https://nodered.org/docs/getting-started/local

For example, install it with npm locally:
````
sudo npm install -g --unsafe-perm node-red
````

### TODO


- all the modules required
  - check also package.json under dependencies
- lodash:
  - ````npm install --save lodash.merge````
- solace utils
  - ````npm install --save ./node-red-contrib-solace-mqtt-app-mgr````

------------------------------------------------------------------------------
The End.
