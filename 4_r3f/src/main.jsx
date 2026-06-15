import React, { useMemo, useRef } from 'react';
import { createRoot } from 'react-dom/client';
import { Canvas, useFrame } from '@react-three/fiber';
import * as THREE from 'three';
import './style.css';

function TriangleModel() {
  const groupRef = useRef();
  const geometry = useMemo(() => {
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute([
       0.0,  0.95, 0.0,
      -0.95, -0.65, 0.0,
       0.95, -0.65, 0.0
    ], 3));
    geo.setAttribute('color', new THREE.Float32BufferAttribute([
      0.92, 0.35, 1.0,
      0.35, 0.90, 1.0,
      1.0, 0.92, 0.35
    ], 3));
    geo.computeVertexNormals();
    return geo;
  }, []);
  const edges = useMemo(() => new THREE.EdgesGeometry(geometry), [geometry]);

  useFrame((_, delta) => {
    groupRef.current.rotation.y += delta * 0.9;
    groupRef.current.rotation.x += delta * 0.35;
  });

  return (
    <group ref={groupRef}>
      <mesh geometry={geometry}>
        <meshStandardMaterial
          vertexColors
          roughness={0.45}
          metalness={0.04}
          side={THREE.DoubleSide}
        />
      </mesh>
      <lineSegments geometry={edges}>
        <lineBasicMaterial color="#17211c" />
      </lineSegments>
    </group>
  );
}

function App() {
  return (
    <Canvas camera={{ position: [0, 0.35, 3.4], fov: 55 }} gl={{ antialias: true }}>
      <color attach="background" args={['#f4f7f2']} />
      <ambientLight intensity={0.75} />
      <directionalLight position={[2.5, 3, 4]} intensity={2.2} />
      <TriangleModel />
    </Canvas>
  );
}

createRoot(document.getElementById('root')).render(<App />);
