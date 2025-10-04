# regex and symbols
text = "abc123xyz"
if text =~ /[a-z]+\d+/i
  puts :matched
end
sym1 = :alpha
sym2 = :"spaces allowed"
sym3 = :'more spaces'
