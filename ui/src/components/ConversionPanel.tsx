import React, { useEffect, useMemo, useState } from 'react';
import { Download, X as XIcon, FolderClosed, File } from './icons';
import { useNativeFunction } from '../hooks/useFunction';
import type { ChainItem, ToneBlock } from '../types/chain';
import { FIELD_BORDER, outlinedFieldStyle, PillToggle } from './controls';
import { MUTED, SUBTLE, WHITE } from './theme';

type ConversionStatus = {
  jobId?: string;
  phase?: string;
  modelName?: string;
  outputPath?: string;
  error?: string;
  finalRmseDb?: number;
  running?: boolean;
  done?: boolean;
  ok?: boolean;
};

type PickResult = { kind?: string; path?: string; cancelled?: boolean };

interface ConversionPanelProps {
  chain: ChainItem[];
  chainRight: ChainItem[] | null | undefined;
  onClose: () => void;
}

const buttonStyle: React.CSSProperties = {
  border: '1rem solid #ffffff',
  borderRadius: '6rem',
  background: 'transparent',
  color: WHITE,
  padding: '9rem 14rem',
  fontSize: '13rem',
  cursor: 'pointer',
  display: 'inline-flex',
  alignItems: 'center',
  justifyContent: 'center',
  gap: '8rem',
};

const fieldStyle: React.CSSProperties = {
  ...outlinedFieldStyle,
  width: '100%',
  padding: '9rem 12rem',
  fontSize: '13rem',
};

const captionStyle: React.CSSProperties = {
  color: MUTED,
  fontSize: '12rem',
  lineHeight: 1.35,
  margin: '4rem 0 0',
};

function isNam(block: ChainItem): block is ToneBlock {
  return block.kind === 'tone' && block.tone.format?.toLowerCase() === 'nam';
}

export const ConversionPanel: React.FC<ConversionPanelProps> = ({ chain, chainRight, onClose }) => {
  const nativeStart = useNativeFunction<{ jobId?: string; error?: string }>('startNamToClo');
  const nativeStatus = useNativeFunction<ConversionStatus>('getNamToCloStatus');
  const pickFile = useNativeFunction<PickResult>('pickConversionFile');

  const namBlocks = useMemo(
    () => [
      ...chain.filter(isNam).map((block) => ({ block, side: 'Left' })),
      ...(chainRight ?? []).filter(isNam).map((block) => ({ block, side: 'Right' })),
    ],
    [chain, chainRight]
  );
  const [selectedBlockId, setSelectedBlockId] = useState('');
  const [destination, setDestination] = useState<'gp200' | 'gp5'>('gp200');
  const [tailMode, setTailMode] = useState<'original' | 'recorded'>('original');
  const [recordedAudio, setRecordedAudio] = useState('');
  const [correctiveIrEnabled, setCorrectiveIrEnabled] = useState(false);
  const [correctiveIr, setCorrectiveIr] = useState('');
  const [referenceWav, setReferenceWav] = useState('');
  const [outputDirectory, setOutputDirectory] = useState('');
  const [jobId, setJobId] = useState('');
  const [status, setStatus] = useState<ConversionStatus | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    if (!selectedBlockId && namBlocks.length > 0) setSelectedBlockId(namBlocks[0].block.blockId);
    if (selectedBlockId && !namBlocks.some(({ block }) => block.blockId === selectedBlockId))
      setSelectedBlockId(namBlocks[0]?.block.blockId ?? '');
  }, [namBlocks, selectedBlockId]);

  useEffect(() => {
    let cancelled = false;
    void nativeStatus('').then((result) => {
      if (cancelled || !result?.jobId) return;
      setJobId(result.jobId);
      setStatus(result);
    });
    return () => {
      cancelled = true;
    };
  }, [nativeStatus]);

  useEffect(() => {
    if (!jobId) return;
    let cancelled = false;
    let timer: number | undefined;
    const poll = async () => {
      const result = await nativeStatus(jobId);
      if (cancelled) return;
      if (result) setStatus(result);
      if (!result?.done) timer = window.setTimeout(() => void poll(), 650);
    };
    void poll();
    return () => {
      cancelled = true;
      if (timer !== undefined) window.clearTimeout(timer);
    };
  }, [jobId, nativeStatus]);

  const selected = namBlocks.find(({ block }) => block.blockId === selectedBlockId)?.block;
  const running = status?.running === true;

  const choose = async (kind: 'recorded' | 'correctiveIr' | 'reference' | 'output') => {
    const result = await pickFile(kind);
    if (!result?.cancelled && result?.path) {
      if (kind === 'recorded') setRecordedAudio(result.path);
      else if (kind === 'correctiveIr') setCorrectiveIr(result.path);
      else if (kind === 'reference') setReferenceWav(result.path);
      else setOutputDirectory(result.path);
    }
  };

  const start = async () => {
    if (!selected || !selected.loaded || running) return;
    setError('');
    setStatus(null);
    const result = await nativeStart({
      blockId: selected.blockId,
      modelName: selected.tone.title,
      destination,
      tailMode: tailMode === 'recorded' ? 'recorded' : 'original',
      recordedAudio,
      correctiveIrEnabled,
      correctiveIr,
      referenceWav,
      outputDirectory,
    });
    if (!result) {
      setError('No se pudo iniciar la conversión.');
    } else if (result.jobId) {
      setJobId(result.jobId);
      setStatus({ jobId: result.jobId, phase: 'queued', running: true, done: false });
    } else if (result.error) {
      setError(result.error);
    }
  };

  const statusText = status?.phase === 'complete'
    ? 'Conversión terminada.'
    : status?.phase === 'failed'
      ? status.error || 'La conversión ha fallado.'
      : status?.phase === 'cancelled'
        ? 'Conversión cancelada.'
      : status?.phase || 'Listo';

  return (
    <div
      style={{
        height: '100%',
        overflow: 'auto',
        boxSizing: 'border-box',
        padding: '20rem 24rem 28rem',
        background: '#050505',
        color: WHITE,
      }}
      className="hide-scrollbar"
    >
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '14rem' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10rem' }}>
          <Download size={20} />
          <div>
            <div style={{ fontSize: '18rem', fontWeight: 600 }}>NAM → CLO</div>
            <div style={{ ...captionStyle, marginTop: '2rem' }}>Conversor oficial integrado · v2.10.1</div>
          </div>
        </div>
        <button type="button" onClick={onClose} style={{ ...buttonStyle, border: 'none', padding: '6rem' }} aria-label="Cerrar conversor">
          <XIcon size={20} />
        </button>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '14rem', alignItems: 'start' }}>
        <section style={{ border: FIELD_BORDER, borderRadius: '8rem', padding: '14rem' }}>
          <div style={{ fontSize: '14rem', fontWeight: 600, marginBottom: '10rem' }}>Modelo y destino</div>
          <label style={{ display: 'block', fontSize: '12rem', color: MUTED, marginBottom: '5rem' }}>NAM cargado en la cadena</label>
          <select value={selectedBlockId} onChange={(event) => setSelectedBlockId(event.target.value)} style={fieldStyle} disabled={running || namBlocks.length === 0}>
            {namBlocks.length === 0 && <option value="">No hay NAM cargados</option>}
            {namBlocks.map(({ block, side }) => (
              <option key={block.blockId} value={block.blockId}>{side} · {block.tone.title}{block.loaded ? '' : ' (cargando)'}</option>
            ))}
          </select>
          <p style={captionStyle}>Se usan los bytes del modelo que ya está cargado; no se realiza una descarga adicional.</p>

          <label style={{ display: 'block', fontSize: '12rem', color: MUTED, margin: '14rem 0 5rem' }}>Destino</label>
          <select value={destination} onChange={(event) => setDestination(event.target.value as 'gp200' | 'gp5')} style={fieldStyle} disabled={running}>
            <option value="gp200">GP-200 · B1024</option>
            <option value="gp5">GP-5 / GP-50 · B512</option>
          </select>
          <p style={captionStyle}>Flujo: B2048 → Corrective IR opcional → reducción → Tone Match directo final.</p>
        </section>

        <section style={{ border: FIELD_BORDER, borderRadius: '8rem', padding: '14rem' }}>
          <div style={{ fontSize: '14rem', fontWeight: 600, marginBottom: '10rem' }}>Estímulo y opciones</div>
          <label style={{ display: 'block', fontSize: '12rem', color: MUTED, marginBottom: '5rem' }}>Cola de 20 segundos</label>
          <select value={tailMode} onChange={(event) => setTailMode(event.target.value as 'original' | 'recorded')} style={fieldStyle} disabled={running}>
            <option value="original">Audio de estímulo original</option>
            <option value="recorded">Audio grabado/reamp</option>
          </select>
          {tailMode === 'recorded' && (
            <div style={{ display: 'flex', gap: '8rem', marginTop: '7rem' }}>
              <input readOnly value={recordedAudio} placeholder="Selecciona un WAV" style={{ ...fieldStyle, flex: 1 }} />
              <button type="button" style={buttonStyle} onClick={() => void choose('recorded')} disabled={running}><File size={15} /> WAV</button>
            </div>
          )}

          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginTop: '14rem' }}>
            <div>
              <div style={{ fontSize: '13rem' }}>Corrective IR</div>
              <p style={captionStyle}>Se aplica sobre B2048 antes del Tone Match.</p>
            </div>
            <PillToggle value={correctiveIrEnabled} onChange={setCorrectiveIrEnabled} disabled={running} />
          </div>
          {correctiveIrEnabled && (
            <div style={{ display: 'flex', gap: '8rem', marginTop: '7rem' }}>
              <input readOnly value={correctiveIr} placeholder="Selecciona un WAV" style={{ ...fieldStyle, flex: 1 }} />
              <button type="button" style={buttonStyle} onClick={() => void choose('correctiveIr')} disabled={running}><File size={15} /> IR</button>
            </div>
          )}

          <label style={{ display: 'block', fontSize: '12rem', color: MUTED, margin: '14rem 0 5rem' }}>Referencia opcional de Tone Match</label>
          <div style={{ display: 'flex', gap: '8rem' }}>
            <input readOnly value={referenceWav} placeholder="Original de 70 s si se deja vacío" style={{ ...fieldStyle, flex: 1 }} />
            <button type="button" style={buttonStyle} onClick={() => void choose('reference')} disabled={running}><File size={15} /> WAV</button>
          </div>
        </section>
      </div>

      <section style={{ border: FIELD_BORDER, borderRadius: '8rem', padding: '14rem', marginTop: '14rem' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '12rem', alignItems: 'center' }}>
          <div>
            <div style={{ fontSize: '14rem', fontWeight: 600 }}>Salida</div>
            <p style={captionStyle}>Si no eliges carpeta se usa “Documentos/TONE3000 CLO”.</p>
          </div>
          <button type="button" style={buttonStyle} onClick={() => void choose('output')} disabled={running}><FolderClosed size={15} /> Elegir carpeta</button>
        </div>
        <input readOnly value={outputDirectory} placeholder="Documentos/TONE3000 CLO" style={{ ...fieldStyle, marginTop: '8rem' }} />
      </section>

      {(error || status) && (
        <div style={{ border: `1rem solid ${status?.ok ? '#2d8a4a' : status?.done ? '#a33' : '#3f3f46'}`, borderRadius: '8rem', padding: '12rem 14rem', marginTop: '14rem' }}>
          <div style={{ fontSize: '13rem' }}>{error || statusText}</div>
          {status?.done && status.ok && status.finalRmseDb !== undefined && Number.isFinite(status.finalRmseDb) && (
            <div style={{ ...captionStyle, marginTop: '6rem' }}>RMSE Tone Match final: {status.finalRmseDb.toFixed(3)} dB</div>
          )}
          {status?.outputPath && <div style={{ ...captionStyle, marginTop: '4rem', wordBreak: 'break-all' }}>{status.outputPath}</div>}
        </div>
      )}

      <div style={{ display: 'flex', justifyContent: 'flex-end', gap: '10rem', marginTop: '16rem' }}>
        <button type="button" style={{ ...buttonStyle, borderColor: SUBTLE, color: MUTED }} onClick={onClose}>Cerrar</button>
        <button type="button" style={{ ...buttonStyle, background: WHITE, color: '#000000' }} onClick={() => void start()} disabled={!selected || !selected.loaded || running}>
          {running ? 'Convirtiendo…' : 'Convertir NAM a CLO'}
        </button>
      </div>
    </div>
  );
};
