
#!/bin/bash

################################################################################
# Generate code maps in JSON for enums in XdkAppInfo.h
#
################################################################################

source ./config.sh.include

CODE_MAPS_INPUT_FILE="$PROJECT_DIR/source/XdkAppInfo.h"
CODE_MAPS_OUTPUT_DIR="$PROJECT_DIR/node-red/code-maps"

# setup for Solace_App_ModuleID_E
SOLACE_APP_MODULE_ID_OUTPUT_FILE="$CODE_MAPS_OUTPUT_DIR/SolaceAppModuleId.json"
SOLACE_APP_MODULE_ID_PREFIX="SOLACE_APP_MODULE_ID_"
echo "[" > $SOLACE_APP_MODULE_ID_OUTPUT_FILE
SolaceAppModuleIdOutputFile_firstLine=1
# setup for Solace_App_Retcode_E
RETCODE_SOLAPP_OUTPUT_FILE="$CODE_MAPS_OUTPUT_DIR/Retcode_SolApp.json"
RETCODE_SOLAPP_PREFIX="RETCODE_SOLAPP_"
echo "[" > $RETCODE_SOLAPP_OUTPUT_FILE
RetcodeSolAppOutputFile_firstLine=1
# setup for AppStatusMessage_StatusCode_E
APP_STATUS_MSG_STATUS_OUTPUT_FILE="$CODE_MAPS_OUTPUT_DIR/AppStatusMessage_Status.json"
APP_STATUS_MSG_STATUS_PREFIX="AppStatusMessage_Status_"
echo "[" > $APP_STATUS_MSG_STATUS_OUTPUT_FILE
AppStatusMsgStatusOutputFile_firstLine=1
# setup for AppStatusMessage_DescrCode_E
APP_STATUS_MSG_DESCR_CODE_OUTPUT_FILE="$CODE_MAPS_OUTPUT_DIR/AppStatusMessage_Descr.json"
APP_STATUS_MSG_DESCR_CODE_PREFIX="AppStatusMessage_Descr_"
echo "[" > $APP_STATUS_MSG_DESCR_CODE_OUTPUT_FILE
AppStatusMsgDescrCodeOutputFile_firstLine=1

clear
echo "###########################################################################"
echo "Generating code maps from"
echo "$CODE_MAPS_INPUT_FILE ..."
echo "###########################################################################"

: '

lib/oracle-11.2.0.3.0.txt
sed 's/.*\(oracle.*\)\.txt/\1/'
returns: oralce-11.2.0.3.0

to extract: 10:26 only:

echo "US/Central - 10:26 PM (CST)" | sed -n "s/^.*-\s*\(\S*\).*$/\1/p"

-n      suppress printing
s       substitute
^.*     anything at the beginning
-       up until the dash
\s*     any space characters (any whitespace character)
\(      start capture group
\S*     any non-space characters
\)      end capture group
.*$     anything at the end
\1      substitute 1st capture group for everything on line
p       print it
'

: '
line="AppStatusMessage_Descr_NULL = 0,																/**< 123 */"
line="AppStatusMessage_Descr_ErrorParsingJsonBefore, 													/**  < 123 */"
line="AppStatusMessage_Descr_ApplyNewMqttBrokerConnectionConfig_NotSupported  ,  $(printf '\t')$(printf '\t')/** < 232*/"
echo "line='$line'"
#echo "$line" | sed -n -E "s|^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX([a-zA-Z_]*),[[:space:]]*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|1:'\1', 2:'\2',rest: |p"
#DESCR=$(echo "$line" | sed -n -E "s|(^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX[[:space:]]*)([a-zA-Z_]*)(,[[:space:]]*/\*\*[[:space:]]*\<[[:space:]]*)([[:digit:]]+).*$|$APP_STATUS_MSG_DESCR_CODE_PREFIX\2|p")
#CODE=$(echo "$line" | sed -n -E "s|(^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX[[:space:]]*)([a-zA-Z_]*)(,[[:space:]]*/\*\*[[:space:]]*\<[[:space:]]*)([[:digit:]]+).*$|\4|p")
echo "$line" | sed -n -E "s|^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|1:'$APP_STATUS_MSG_DESCR_CODE_PREFIX\1', 2:'\2', rest: |p"
DESCR=$(echo "$line" | sed -n -E "s|^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|$APP_STATUS_MSG_DESCR_CODE_PREFIX\1|p")
CODE=$(echo "$line" | sed -n -E "s|^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|\2|p")
echo "DESCR=$DESCR"
echo "CODE=$CODE"
exit
'

PAYLOAD='
{
  "timestamp": "'"$TIMESTAMP"'",
  "exchangeId": "'"$EXCHANGE_ID"'",
  "command": "'"$COMMAND"'",
  "delay": 1
}
'
#
# usage:
# $(parseAndAppend2File "$outputFile" "$prefix" "$line" "$isFirstLine")
function parseAndAppend2File() {

  #echo "$1, $2" > /dev/tty
  #echo "line=$3" > /dev/tty
  #echo "isFirstLine=$4" > /dev/tty

  if [[ $# != 4 ]]; then
      echo "Usage: parseAndAppend2File(outputFile prefix line isFirstLine)" > /dev/tty
      echo 1; return 1
  fi
  local outputFile=$1
  local prefix=$2
  local line=$3
  local isFirstLine=$4

  #echo "$line" | sed -n -E "s|^.*$APP_STATUS_MSG_DESCR_CODE_PREFIX([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|1:'$APP_STATUS_MSG_DESCR_CODE_PREFIX\1', 2:'\2', rest: |p"
  DESCR=$(echo "$line" | sed -n -E "s|^.*$prefix([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|$prefix\1|p")
  CODE=$(echo "$line" | sed -n -E "s|^.*$prefix([a-zA-Z_]*).*/\*\*[[:space:]]*\<[[:space:]]*([[:digit:]]+).*$|\2|p")

  echo "CODE=$CODE, DESCR=$DESCR" > /dev/tty

  if [ -z "$DESCR" ]; then echo 2; return 1; fi
  if [ -z "$CODE" ]; then echo 2; return 1; fi

  if [ $isFirstLine -eq 1 ] ; then
    echo '['"$CODE"',"'"$DESCR"'"]' >> $outputFile
  else
    echo ',['"$CODE"',"'"$DESCR"'"]' >> $outputFile
  fi

  echo 0
}

while IFS= read -r line
do

  if grep -q "$SOLACE_APP_MODULE_ID_PREFIX" <<<"$line"; then
    call=$(parseAndAppend2File $SOLACE_APP_MODULE_ID_OUTPUT_FILE $SOLACE_APP_MODULE_ID_PREFIX "$line" $SolaceAppModuleIdOutputFile_firstLine)
    if [ $call -eq 0 ]; then SolaceAppModuleIdOutputFile_firstLine=0; fi
  fi
  if grep -q "$RETCODE_SOLAPP_PREFIX" <<<"$line"; then
    call=$(parseAndAppend2File $RETCODE_SOLAPP_OUTPUT_FILE $RETCODE_SOLAPP_PREFIX "$line" $RetcodeSolAppOutputFile_firstLine)
    if [ $call -eq 0 ]; then RetcodeSolAppOutputFile_firstLine=0; fi
  fi
  if grep -q "$APP_STATUS_MSG_STATUS_PREFIX" <<<"$line"; then
    call=$(parseAndAppend2File $APP_STATUS_MSG_STATUS_OUTPUT_FILE $APP_STATUS_MSG_STATUS_PREFIX "$line" $AppStatusMsgStatusOutputFile_firstLine)
    if [ $call -eq 0 ]; then AppStatusMsgStatusOutputFile_firstLine=0; fi
  fi
  if grep -q "$APP_STATUS_MSG_DESCR_CODE_PREFIX" <<<"$line"; then
    call=$(parseAndAppend2File $APP_STATUS_MSG_DESCR_CODE_OUTPUT_FILE $APP_STATUS_MSG_DESCR_CODE_PREFIX "$line" $AppStatusMsgDescrCodeOutputFile_firstLine)
    if [ $call -eq 0 ]; then AppStatusMsgDescrCodeOutputFile_firstLine=0; fi
  fi

done < "$CODE_MAPS_INPUT_FILE"

echo "]" >> $SOLACE_APP_MODULE_ID_OUTPUT_FILE
echo "]" >> $RETCODE_SOLAPP_OUTPUT_FILE
echo "]" >> $APP_STATUS_MSG_STATUS_OUTPUT_FILE
echo "]" >> $APP_STATUS_MSG_DESCR_CODE_OUTPUT_FILE


echo
echo "done."
echo



# The End.
