using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class MidiProbe
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct MIDIINCAPS { public ushort wMid, wPid; public uint vDriverVersion; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname; public uint dwSupport; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct MIDIOUTCAPS { public ushort wMid, wPid; public uint vDriverVersion; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string szPname; public ushort wTechnology, wVoices, wNotes, wChannelMask; public uint dwSupport; }
    [StructLayout(LayoutKind.Sequential)]
    struct MIDIHDR { public IntPtr lpData; public uint dwBufferLength, dwBytesRecorded; public IntPtr dwUser; public uint dwFlags; public IntPtr lpNext; public IntPtr reserved; public uint dwOffset; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public IntPtr[] dwReserved; }

    [DllImport("winmm.dll")] static extern uint midiInGetNumDevs();
    [DllImport("winmm.dll")] static extern uint midiOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)] static extern uint midiInGetDevCaps(UIntPtr id, out MIDIINCAPS caps, uint size);
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)] static extern uint midiOutGetDevCaps(UIntPtr id, out MIDIOUTCAPS caps, uint size);
    [DllImport("winmm.dll")] static extern uint midiInOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] static extern uint midiOutOpen(out IntPtr h, uint id, IntPtr cb, IntPtr inst, uint flags);
    [DllImport("winmm.dll")] static extern uint midiInPrepareHeader(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiInUnprepareHeader(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiInAddBuffer(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiInStart(IntPtr h);
    [DllImport("winmm.dll")] static extern uint midiInStop(IntPtr h);
    [DllImport("winmm.dll")] static extern uint midiInReset(IntPtr h);
    [DllImport("winmm.dll")] static extern uint midiInClose(IntPtr h);
    [DllImport("winmm.dll")] static extern uint midiOutPrepareHeader(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiOutUnprepareHeader(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiOutLongMsg(IntPtr h, IntPtr hdr, uint size);
    [DllImport("winmm.dll")] static extern uint midiOutClose(IntPtr h);

    delegate void MidiInProc(IntPtr h, uint msg, IntPtr inst, IntPtr p1, IntPtr p2);
    [DllImport("winmm.dll")] static extern uint midiInOpen(out IntPtr h, uint id, MidiInProc cb, IntPtr inst, uint flags);
    static MidiInProc keepAlive;
    static readonly List<string> heard = new List<string>();
    static DateTime t0;

    // Logs every short message (CC, notes, program change...) arriving on inId for ms milliseconds.
    // One line per message: time ms, channel, kind, data. Sends nothing.
    public static string Listen(uint inId, int ms)
    {
        heard.Clear();
        t0 = DateTime.Now;
        keepAlive = (h, msg, inst, p1, p2) =>
        {
            if (msg != 0x3C3) return;  // MIM_DATA
            uint m = (uint)p1.ToInt64();
            int st = (int)(m & 0xFF), d1 = (int)((m >> 8) & 0x7F), d2 = (int)((m >> 16) & 0x7F);
            if (st == 0xF8 || st == 0xFE) return;  // clock, active sensing
            string kind = (st & 0xF0) == 0xB0 ? "CC " + d1 + " = " + d2 : (st & 0xF0) == 0xC0 ? "PC " + d1 : st.ToString("X2") + " " + d1 + " " + d2;
            lock (heard) heard.Add(((int)(DateTime.Now - t0).TotalMilliseconds).ToString().PadLeft(6) + "  ch" + ((st & 0x0F) + 1) + "  " + kind);
        };
        IntPtr hin;
        uint r = midiInOpen(out hin, inId, keepAlive, IntPtr.Zero, 0x30000);  // CALLBACK_FUNCTION
        if (r != 0) return "midiInOpen failed " + r;
        midiInStart(hin);
        Thread.Sleep(ms);
        midiInStop(hin);
        midiInReset(hin);
        midiInClose(hin);
        lock (heard) return string.Join("\n", heard);
    }

    const uint MHDR_DONE = 1;
    const int BUF = 4096, NBUF = 8;

    public static string List()
    {
        var sb = new StringBuilder();
        for (uint i = 0; i < midiInGetNumDevs(); i++) { MIDIINCAPS c; midiInGetDevCaps((UIntPtr)i, out c, (uint)Marshal.SizeOf(typeof(MIDIINCAPS))); sb.AppendLine("IN  " + i + ": " + c.szPname); }
        for (uint i = 0; i < midiOutGetNumDevs(); i++) { MIDIOUTCAPS c; midiOutGetDevCaps((UIntPtr)i, out c, (uint)Marshal.SizeOf(typeof(MIDIOUTCAPS))); sb.AppendLine("OUT " + i + ": " + c.szPname); }
        return sb.ToString();
    }

    static byte[] Hex(string s)
    {
        var parts = s.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        var b = new byte[parts.Length];
        for (int i = 0; i < parts.Length; i++) b[i] = Convert.ToByte(parts[i], 16);
        return b;
    }

    // Sends each request (hex string) and collects all SysEx replies for waitMs after it.
    public static string Ask(uint inId, uint outId, string[] requests, int waitMs, string outDir)
    {
        var log = new StringBuilder();
        IntPtr hin, hout;
        uint r = midiInOpen(out hin, inId, IntPtr.Zero, IntPtr.Zero, 0);
        if (r != 0) return "midiInOpen failed " + r;
        r = midiOutOpen(out hout, outId, IntPtr.Zero, IntPtr.Zero, 0);
        if (r != 0) { midiInClose(hin); return "midiOutOpen failed " + r; }
        int hsz = Marshal.SizeOf(typeof(MIDIHDR));
        var hdrs = new IntPtr[NBUF];
        for (int i = 0; i < NBUF; i++)
        {
            var h = new MIDIHDR { lpData = Marshal.AllocHGlobal(BUF), dwBufferLength = BUF, dwReserved = new IntPtr[8] };
            hdrs[i] = Marshal.AllocHGlobal(hsz);
            Marshal.StructureToPtr(h, hdrs[i], false);
            midiInPrepareHeader(hin, hdrs[i], (uint)hsz);
            midiInAddBuffer(hin, hdrs[i], (uint)hsz);
        }
        midiInStart(hin);
        var cur = new List<byte>();
        int n = 0;
        foreach (var req in requests)
        {
            var msg = Hex(req);
            log.AppendLine(">> " + req);
            IntPtr data = Marshal.AllocHGlobal(msg.Length);
            Marshal.Copy(msg, 0, data, msg.Length);
            var oh = new MIDIHDR { lpData = data, dwBufferLength = (uint)msg.Length, dwBytesRecorded = (uint)msg.Length, dwReserved = new IntPtr[8] };
            IntPtr ohp = Marshal.AllocHGlobal(hsz);
            Marshal.StructureToPtr(oh, ohp, false);
            midiOutPrepareHeader(hout, ohp, (uint)hsz);
            midiOutLongMsg(hout, ohp, (uint)hsz);
            var until = DateTime.Now.AddMilliseconds(waitMs);
            while (DateTime.Now < until)
            {
                for (int i = 0; i < NBUF; i++)
                {
                    var h = (MIDIHDR)Marshal.PtrToStructure(hdrs[i], typeof(MIDIHDR));
                    if ((h.dwFlags & MHDR_DONE) == 0) continue;
                    var got = new byte[h.dwBytesRecorded];
                    Marshal.Copy(h.lpData, got, 0, got.Length);
                    foreach (var b in got)
                    {
                        if (b == 0xF0) cur.Clear();
                        cur.Add(b);
                        if (b == 0xF7)
                        {
                            n++;
                            var m = cur.ToArray();
                            File.WriteAllBytes(Path.Combine(outDir, "reply" + n + ".syx"), m);
                            var hx = new StringBuilder();
                            for (int k = 0; k < Math.Min(m.Length, 40); k++) hx.Append(m[k].ToString("X2")).Append(' ');
                            log.AppendLine("<< reply" + n + " len=" + m.Length + ": " + hx + (m.Length > 40 ? "..." : ""));
                            cur.Clear();
                        }
                    }
                    midiInUnprepareHeader(hin, hdrs[i], (uint)hsz);
                    h.dwFlags = 0; h.dwBytesRecorded = 0;
                    Marshal.StructureToPtr(h, hdrs[i], false);
                    midiInPrepareHeader(hin, hdrs[i], (uint)hsz);
                    midiInAddBuffer(hin, hdrs[i], (uint)hsz);
                }
                Thread.Sleep(5);
            }
            midiOutUnprepareHeader(hout, ohp, (uint)hsz);
        }
        midiInStop(hin);
        midiInReset(hin);
        for (int i = 0; i < NBUF; i++) midiInUnprepareHeader(hin, hdrs[i], (uint)hsz);
        midiInClose(hin);
        midiOutClose(hout);
        return log.ToString();
    }
}
