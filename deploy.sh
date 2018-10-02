#!/bin/sh
FIXED_BRANCH=$(echo $BRANCH | sed 's/\//-/g')
ARCHIVE=$REPO_NAME-$FIXED_BRANCH-$(date +%Y-%m-%d_%H_%M_%S)-$COMMIT.tar.bz2
echo "Creating archive $ARCHIVE"
tar cfj $ARCHIVE build/bin
FILESIZE=$(stat -c%s "$ARCHIVE")
echo "Finished archive (size $FILESIZE), starting Google Drive upload"
chmod u+x ./gdrive
./gdrive upload --refresh-token $GDRIVE_REFRESH_TOKEN --parent $GDRIVE_DIR "$ARCHIVE"
echo "Finished Google Drive upload"
