#!/usr/bin/env ruby

# Very basic Ruby script to post to a phant instance.
# This is unlikely to be very idiomatic; better examples
# welcome.

require 'net/http'

endpoint = 'https://data.sparkfun.com'
pubhash  = 'EJJDW2GaoRtVnWN28bZE'
privhash = 'dqqkE7W6w8Hgbzjw6GmJ'

data = { }

data['temp'] = ARGV.first

path = "#{endpoint}/input/#{pubhash}"
uri = URI(path)

req = Net::HTTP::Post.new(uri.path)
req.set_form_data(data)
req['Phant-Private-Key'] = privhash

res = Net::HTTP.start(uri.hostname, uri.port, :use_ssl => true) do |http|
  http.request(req)
end

puts res.body