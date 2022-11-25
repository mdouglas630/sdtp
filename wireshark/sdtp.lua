-- SDTP Protocol dissector for wireshark


-- declaring our SDTP protocol
sdtp_proto = Proto("sdtp","SDTP Protocol")

-- declaring SDTP fields
seqnum = ProtoField.uint16("sdtp.seqnum", "seqnum", base.DEC)
acknum = ProtoField.uint16("sdtp.acknum", "acknum", base.DEC)
datalen = ProtoField.uint8("sdtp.datalen", "datalen", base.DEC)
flags = ProtoField.uint8("sdtp.flags", "flags", base.HEX)
window = ProtoField.uint16("sdtp.window", "window", base.DEC)
checksum = ProtoField.uint16("sdtp.checksum", "checksum", base.HEX)
data = ProtoField.string("sdtp.data", "data", base.ASCII)

-- adding fields
sdtp_proto.fields = { seqnum, acknum, datalen, flags, window, checksum, data }

-- testing if a flag is defined
function test_flag(flag, code)
    retcode = 0
    if bit.band(flag, code) == code then
        retcode = 1
    end
    return retcode
end

-- getting the flags name
function get_flag_name(flag)
  local flag_name = ""

  if test_flag(flag, 0x1) == 1 then flag_name = flag_name .. "FIN " end
  if test_flag(flag, 0x2) == 1 then flag_name = flag_name .. "SYN " end
  if test_flag(flag, 0x4) == 1 then flag_name = flag_name .. "RST " end
  if test_flag(flag, 0x8) == 1 then flag_name = flag_name .. "PUSH " end
  if test_flag(flag, 0x10) == 1 then flag_name = flag_name .. "ACK " end
  if test_flag(flag, 0x20) == 1 then flag_name = flag_name .. "URG " end

  return flag_name
end

-- create a function to dissect it
function sdtp_proto.dissector(buffer,pinfo,tree)
    
    pinfo.cols.protocol = "SDTP"

    local subtree = tree:add(sdtp_proto,buffer(),"SDTP - Simple Data Transfer Protocol")
    
    local datalength = buffer(4,1):le_uint()

    local flags_name = get_flag_name(buffer(5,1):uint())

    subtree:add_le(seqnum, buffer(0,2))
    subtree:add_le(acknum, buffer(2,2))
    subtree:add_le(datalen, buffer(4,1))
    subtree_flags = subtree:add_le(flags, buffer(5,1)):append_text(" (" .. flags_name:sub(1, -2) .. ")")
    
    -- getting the flags
    if (test_flag(buffer(5,1):uint(), 0x1) == 1) then
        subtree_flags:add(buffer(5,1),".......1: FIN: Set")
    else
        subtree_flags:add(buffer(5,1),".......0: FIN: Not set")
    end
    
    if (test_flag(buffer(5,1):uint(), 0x2) == 1) then
        subtree_flags:add(buffer(5,1),"......1.: SYN: Set")
    else
        subtree_flags:add(buffer(5,1),"......0.: SYN: Not set")
    end

    if (test_flag(buffer(5,1):uint(), 0x4) == 1) then
        subtree_flags:add(buffer(5,1),".....1..: RST: Set")
    else
        subtree_flags:add(buffer(5,1),".....0..: RST: Not set")
    end
    
    if (test_flag(buffer(5,1):uint(), 0x8) == 1) then
        subtree_flags:add(buffer(5,1),"....1...: PUSH: Set")
    else
        subtree_flags:add(buffer(5,1),"....0...: PUSH: Not set")
    end
    
    if (test_flag(buffer(5,1):uint(), 0x10) == 1) then
        subtree_flags:add(buffer(5,1),"...1....: ACK: Set")
    else
        subtree_flags:add(buffer(5,1),"...0....: ACK: Not set")
    end
    
    if (test_flag(buffer(5,1):uint(), 0x20) == 1) then
        subtree_flags:add(buffer(5,1),"..1.....: URG: Set")
    else
        subtree_flags:add(buffer(5,1),"..0.....: URG: Not set")
    end

    subtree:add_le(window, buffer(6,2))
    subtree:add(checksum, buffer(8,2))

    if datalength > 0 then
        subtree:add_le(data, buffer(10,datalength))
    end

end

-- load the udp.port table
udp_table = DissectorTable.get("udp.port")

-- register our protocol to handle udp port 21020
udp_table:add(21020,sdtp_proto)


-- reference: 
-- part 1: https://mika-s.github.io/wireshark/lua/dissector/2017/11/04/creating-a-wireshark-dissector-in-lua-1.html
-- part 2: https://mika-s.github.io/wireshark/lua/dissector/2017/11/06/creating-a-wireshark-dissector-in-lua-2.html
-- part 3: https://mika-s.github.io/wireshark/lua/dissector/2017/11/08/creating-a-wireshark-dissector-in-lua-3.html

