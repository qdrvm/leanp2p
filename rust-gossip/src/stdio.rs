use anyhow::anyhow;
use tokio::io::AsyncBufReadExt;
use tokio::io::BufReader;
use tokio_fd::AsyncFd;

pub struct Stdin(BufReader<AsyncFd>);
impl Stdin {
    pub fn new() -> anyhow::Result<Self> {
        let fd = AsyncFd::try_from(libc::STDIN_FILENO)?;
        Ok(Self(BufReader::new(fd)))
    }
    pub async fn read(&mut self) -> anyhow::Result<String> {
        let mut s = String::new();
        if self.0.read_line(&mut s).await? == 0 {
            return Err(anyhow!("stdin eof"));
        }
        while s.ends_with('\n') {
            s.pop();
        }
        Ok(s)
    }
}
