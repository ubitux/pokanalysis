use std::fmt;

#[derive(Default, Debug, Clone, Copy, PartialEq)]
pub struct Addr {
    pub bank: u8,
    pub addr: u16,
}

impl Addr {
    pub const fn new(bank: u8, addr: u16) -> Self {
        Self { bank, addr }
    }

    pub fn pos(&self) -> usize {
        let addr = self.addr as usize;
        if addr > 0x3FFF {
            self.bank as usize * 0x4000 + addr - 0x4000
        } else {
            // assert_eq!(self.bank, 0);
            addr
        }
    }

    pub fn add(&mut self, off: u16) -> Self {
        if self.addr > 0x3FFF {
            assert!((self.addr as usize) + (off as usize) < 0x8000);
        } else {
            // assert_eq!(self.bank, 0);
            assert!((self.addr as usize) + (off as usize) < 0x4000);
        }
        self.addr += off;
        *self
    }
}

impl fmt::Display for Addr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:02x}:{:04x}", self.bank, self.addr)
    }
}

pub struct Reader<'a> {
    pub stream: &'a Vec<u8>,
    pub addr: Addr,
}

impl<'a> Reader<'a> {
    pub fn new_at(stream: &'a Vec<u8>, addr: Addr) -> Self {
        Self { stream, addr }
    }

    pub fn read_u8(&mut self) -> u8 {
        // XXX risk of overread, should probably raise an error
        let ret = self.stream[self.addr.pos()];
        self.addr.add(1);
        ret
    }

    pub fn read_u8s<const N: usize>(&mut self) -> [u8; N] {
        let mut dst = [0; N];
        for i in 0..N {
            dst[i] = self.read_u8();
        }
        dst
    }

    pub fn read_u16(&mut self) -> u16 {
        let a = self.read_u8();
        let b = self.read_u8();
        u16::from_le_bytes([a, b])
    }

    pub fn seek(&mut self, addr: Addr) {
        self.addr = addr;
    }

    pub fn skip(&mut self, offset: u16) {
        self.addr.add(offset);
    }
}

// XXX why do I need a const generic here?
pub struct BlobSlicer<'a, const N: usize> {
    stream: &'a Vec<u8>,
    addr: Addr,
}

impl<'a, const N: usize> BlobSlicer<'a, N> {
    pub fn new_at(stream: &'a Vec<u8>, addr: Addr) -> Self {
        Self { stream, addr }
    }

    pub fn slice_at(&self, slice_index: u8) -> &[u8; N] {
        let pos = self.addr.pos() + slice_index as usize * N;
        self.stream[pos..pos + N].try_into().unwrap()
    }
}
