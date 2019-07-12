#!/usr/bin/env bash -e

# Publish a file to an s3 bucket and rebuild the index.html

REPORTFILE=$1
BUCKETPATH=${2:-s3://sfc-fdb-build/fdb-results}
BUCKETPATH=$(echo $BUCKETPATH | sed -e 's:/*$::')

INDEX="index.html"

if [[ -z $REPORTFILE ]] || [[ -z $BUCKETPATH ]]; then
    >&2 echo Must specify a reportfile and a path
    exit 1
fi
FILENAME=$(basename $REPORTFILE)

aws s3 cp --only-show-errors $REPORTFILE $BUCKETPATH/$FILENAME

cat > $INDEX<<EOF
<HTML>
<HEAD><TITLE>FDB</TITLE></HEAD>
<BODY>
EOF
aws s3 ls $BUCKETPATH/ | grep -v index.html | awk '{print "<A HREF="$NF">"$NF"</A><BR>"}' >> $INDEX
cat >> $INDEX<<EOF
</BODY>
</HTML>
EOF

aws s3 cp --only-show-errors $INDEX $BUCKETPATH/index.html
