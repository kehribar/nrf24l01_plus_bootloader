#!/usr/bin/env python
 
import os
import sys
import xively
import subprocess
import time
import datetime
import requests
 
# extract feed_id and api_key from environment variables
FEED_ID = FILL_THIS
API_KEY = FILL_THIS
DEBUG = 1
 
# initialize api client
api = xively.XivelyAPIClient(API_KEY)
 
# function to return a datastream object. This either creates a new datastream,
# or returns an existing one
def get_datastream(feed):
  try:
    datastream = feed.datastreams.get("readout")
    if DEBUG:
      print "Found existing datastream"
    return datastream
  except:
    if DEBUG:
      print "Creating new datastream"
    datastream = feed.datastreams.create("readout", tags="readout_01")
    return datastream
 
def run():
  print "Starting Xively tutorial script"
 
  feed = api.feeds.get(FEED_ID)
 
  datastream = get_datastream(feed)
  datastream.max_value = None
  datastream.min_value = None
 
  send_val = sys.argv[1]

  if DEBUG:
    print "Updating Xively feed with value: %s" % send_val

  datastream.current_value = send_val
  datastream.at = datetime.datetime.utcnow()
  try:
    datastream.update()
  except requests.HTTPError as e:
    print "HTTPError({0}): {1}".format(e.errno, e.strerror)

  return 0
 
run()