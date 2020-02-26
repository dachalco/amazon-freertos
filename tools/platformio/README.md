./repo
    the "online" repo that new packages are downloaded from


./mods
    modified files that need to overwrite corresponding files in user's ~/.platformio

    Note: ~/.platformio is where pio stores its downloaded content

./scripts
    platformio build scripts

TODO: 
dl-packages should ideally include a platformio-freertos.tar.gz
so that the package manifest can link to a web-resource. However said tar.gz
was to big to upload to git so instead the modified espressif package
manifest.json links to a local file via 'file://*'. That local file link
needs to move to a path that is user-agnostic. Currently it points to 
a file path specific to my machine. In the end, users will need to be able
to download the tar.gz from somewhere