## Debugging

Run cmake in the Debug mode for debugging symbols and logging:

    cmake -DCMAKE_BUILD_TYPE=Debug ..

Run fcitx in non-daemon mode to see logging messages:

    fcitx --replace -D

## bogo-python

bogo-python was added by this command:

    git subtree add -P bogo-python --squash https://github.com/BoGoEngine/bogo-python.git master

So now basically the bogo-python sub-project is committed directly into the source tree. To update it:

    git subtree pull --prefix=bogo-python --squash https://github.com/BoGoEngine/bogo-python.git master

