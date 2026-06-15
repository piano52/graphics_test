import * as THREE from 'three';
import './style.css';

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x17110a);

const camera = new THREE.PerspectiveCamera(55, window.innerWidth / window.innerHeight, 0.1, 100);
camera.position.set(0, 0.35, 3.4);

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const keyLight = new THREE.DirectionalLight(0xffffff, 2.2);
keyLight.position.set(2.5, 3.0, 4.0);
scene.add(keyLight);
scene.add(new THREE.AmbientLight(0xffd9a8, 0.55));

const geometry = new THREE.BufferGeometry();
geometry.setAttribute('position', new THREE.Float32BufferAttribute([
   0.0,  0.95, 0.0,
  -0.95, -0.65, 0.0,
   0.95, -0.65, 0.0
], 3));
geometry.setAttribute('color', new THREE.Float32BufferAttribute([
  1.0, 0.88, 0.15,
  1.0, 0.20, 0.62,
  0.15, 0.95, 0.90
], 3));
geometry.computeVertexNormals();

const model = new THREE.Mesh(
  geometry,
  new THREE.MeshStandardMaterial({
    vertexColors: true,
    roughness: 0.48,
    metalness: 0.08,
    side: THREE.DoubleSide
  })
);

const edgeLines = new THREE.LineSegments(
  new THREE.EdgesGeometry(geometry),
  new THREE.LineBasicMaterial({ color: 0xfff1c2 })
);
model.add(edgeLines);
scene.add(model);

function resize() {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
}
window.addEventListener('resize', resize);

function animate() {
  model.rotation.y += 0.018;
  model.rotation.x += 0.006;
  renderer.render(scene, camera);
  requestAnimationFrame(animate);
}
animate();
