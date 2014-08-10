try:
    import pyuv
except ImportError:
    print("PYUV not installed! We needs it for sane networking operations! Run [pip3 install pyuv] as root please.")
    import derpyuv as pyuv

import commands,time

import socket,sys,logging,signal
import re

logging.basicConfig(level=logging.DEBUG)

whitespace = re.compile("\\s+")

def maybeAlias(who):
    return who

class UDPInfo:
    low = 0
    high = 65528
    curr = 65527

class Types:
    FULL = 0
    END = 0 # same as FULL but eh w/ev
    START = 1
    CONTINUE = 2
    # FILE = 3 etc...

# as messages may be bigger than 65K (quite an essay, but still possible)
# we need a superset fragmentation scheme, in addition to the normal one which
# maxes out at 65K. Redundant, but necessary...

class CompleteSender:
    started = False
    def __init__(self,proto,addr,message):
        self.message = message
        self.proto = proto
        self.send()
    def send(self):
        message = self.message[:UDPInfo.curr-1]
        if self.started:
            if len(message) == len(self.message):
                kind = Types.END
            else:
                kind = Types.CONTINUE
        else:
            if len(message) == len(self.message):
                kind = Types.FULL
            else:
                kind = Types.START
        message = bytes([kind])+message
        proto.send(addr,message,lambda proto,status: self.sent(proto,status,len(message)))
    def sent(self,proto,status,message):
        if status != 0:
            # oops, UDP Info is wrong!
            logging.error("Oops, tried to send a message piece too big!")
            UDPInfo.high = num
            UDPInfo.curr = int((UDPInfo.high +UDPInfo.low)/2)
            self.send()
        else:
            self.message = self.message[num:]
            UDPInfo.low = max(num,UDPInfo.low)
            if self.message:
                self.send()
            else:
                self.done()
            if UDPInfo.low != UDPInfo.high + 1:
                UDPInfo.curr = (UDPInfo.high + UDPInfo.low) / 2
    def done(self):
        logging.debug("Message has been sent")

class Protocol(pyuv.UDP):
    def __init__(self,loop,addr,port):
        self.ports = {}
        self.friends = set()
        self.ibuffer = {}
        super().__init__(loop)
        self.bind((addr,port))
        self.start_recv(self.handle_read)
    def handle_read(self,handle,addr,flags,data,error):
        if not addr[0] in self.friends:
            logging.debug("ignoring {} from [{}]:{}".format(len(data),addr[0],addr[1]))
            return
        self.ports[addr[0]] = addr[1]
        addr = addr[0]
        buf = self.ibuffer.get(addr,b"")
        buf += data
        messages = buf.split(b"\0")
        buf = messages[-1]
        messages = messages[:-1]
        self.ibuffer[addr] = buf
        for message in messages:
            self.handle_message(addr,message)
    def found_terminator(self):
        line = "".join(self.ibuffer)
    def send(self,addr,message):
        CompleteSender(super(),addr,message)
    def handle_message(self,who,message):
        self.lastAddr = who
        kind = message[0]
        message = message[1:]
        if kind == 0:
            print("<"+maybeAlias(who)+"> "+message.decode('utf-8'))
        elif kind == 1:
            print("<"+maybeAlias(who)+"> file transfer initiate derp")
        else
            raise RuntimeError("What kind is {:d}?".format(kind))

class ConsoleHandler(pyuv.TTY):
    def __init__(self,loop,protocol):
        self.protocol = protocol
        self.buffer = b""
        super().__init__(loop,sys.stdin.fileno(), True)
        badsigs = [signal.SIGINT,signal.SIGHUP,signal.SIGQUIT]
        self.signals = [pyuv.Signal(loop) for derp in badsigs]
        for i,sig in enumerate(badsigs):
            self.signals[i].start(self.onSignal,sig)
        self.start_read(self.handle_read)
    def handle_read(self,flags,data,error):
        if data is None:
            self.close()
            return
        self.buffer += data
        lines = self.buffer.decode('utf-8').split("\n")
        buf = lines[-1]
        lines = lines[:-1]
        self.buffer = buf.encode('utf-8')
        for line in lines:
            self.handle_line(line)
    def handle_line(self,line):
        if line.startswith("/"):
            try: command,args = whitespace.split(line[1:],1)
            except ValueError:
                command = line[1:]
                args = ()
            commands.run(command,self,args)
        else:
            if not self.protocol.friends:
                print("You have no friends. :(")
            else:
                logging.debug("Sending to {} friends".format(len(self.protocol.friends)))
                for addr in self.protocol.friends:
                    print(repr(addr))
                    self.protocol.send(addr,line.encode('utf-8'))
    def close(self):
        for sig in self.signals:
            sig.close()
        super().close()
        raise SystemExit
    def onSignal(self,handle,signum):
        self.close()

def trySetup(addr,port):
    loop = pyuv.Loop.default_loop()
    proto = Protocol(loop,addr,port)
    stdin = ConsoleHandler(loop,proto)
    print("Tell your friends /add [{}]:{}".format(addr,port))
    with commands.init(stdin):
        loop.run()
